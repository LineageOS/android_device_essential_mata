/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/qseecom.h>
#include <linux/msm_ion.h>

/* Service IDs */
#define SCM_SVC_SSD                 0x07

/* Service specific command IDs */
#define SSD_PARSE_MD_ID             0x06
#define SSD_DECRYPT_IMG_FRAG_ID     0x07

/* SSD parsing status messages from TZ */
#define SSD_PMD_ENCRYPTED           0
#define SSD_PMD_NOT_ENCRYPTED       1
#define SSD_PMD_PARSING_INCOMPLETE  6

#define SSD_HEADER_MIN_SIZE         128
#define MULTIPLICATION_FACTOR       2

#define SMCMOD_DECRYPT_REQ_OP_METADATA  1
#define SMCMOD_DECRYPT_REQ_OP_IMG_FRAG  2

struct smcmod_decrypt_req {
    uint32_t service_id; /* in */
    uint32_t command_id; /* in */
    uint32_t operation;  /* in */

    union {
        struct {
            uint32_t len;
            uint32_t ion_fd;
        } metadata;
        struct {
            uint32_t ctx_id;
            uint32_t last_frag;
            uint32_t frag_len;
            uint32_t ion_fd;
            uint32_t offset;
        } img_frag;
    } request;

    union {
        struct {
            uint32_t status;
            uint32_t ctx_id;
            uint32_t end_offset;
        } metadata;
        struct {
            uint32_t status;
        } img_frag;
    } response;
};

#define SMCMOD_IOC_MAGIC    0x97
#define SMCMOD_IOCTL_DECRYPT_CMD \
    _IOWR(SMCMOD_IOC_MAGIC, 37, struct smcmod_decrypt_req)

struct ion_buf_handle {
    unsigned char *buffer;
    uint32_t buffer_len;
    int ion_fd;
    int ifd_data_fd;
    struct ion_handle_data ion_alloc_handle;
};

static int
ion_memalloc(struct ion_buf_handle *buf, uint32_t size, uint32_t heap)
{
    struct ion_allocation_data alloc_data;
    struct ion_fd_data fd_data;
    unsigned char *va;
    struct ion_handle_data handle_data;
    int ion_fd;
    int rc;

    ion_fd = open("/dev/ion", O_RDONLY);
    if (ion_fd < 0) {
        fprintf(stderr, "Cannot open ION device (%s)\n", strerror(errno));
        return -1;
    }

    alloc_data.len = (size + 4095) & ~4095;
    alloc_data.align = 4096;

    alloc_data.flags = 0;
    alloc_data.heap_id_mask = ION_HEAP(heap);

    /* Set the buffers to be uncached */
    alloc_data.flags = 0;

    rc = ioctl(ion_fd, ION_IOC_ALLOC, &alloc_data);
    if (rc) {
        fprintf(stderr, "ION buffer allocation failed (%s)\n",
                strerror(errno));
        goto alloc_fail;
    }

    if (alloc_data.handle) {
        fd_data.handle = alloc_data.handle;
    } else {
        fprintf(stderr, "ION alloc data returned NULL\n");
        rc = -1;
        goto alloc_fail;
    }

    rc = ioctl(ion_fd, ION_IOC_MAP, &fd_data);
    if (rc) {
        fprintf(stderr, "ION map call failed(%s)\n", strerror(errno));
        goto ioctl_fail;
    }

    va = (unsigned char*)mmap(NULL, alloc_data.len, PROT_READ | PROT_WRITE,
              MAP_SHARED, fd_data.fd, 0);
    if (va == MAP_FAILED) {
        fprintf(stderr, "ION memory map failed (%s)\n", strerror(errno));
        rc = -1;
        goto map_fail;
    }

    buf->ion_fd = ion_fd;
    buf->ifd_data_fd = fd_data.fd;
    buf->buffer = va;
    buf->ion_alloc_handle.handle = alloc_data.handle;
    buf->buffer_len = alloc_data.len;

    memset(buf->buffer, 0, buf->buffer_len);
    return 0;

map_fail:
ioctl_fail:
    handle_data.handle = alloc_data.handle;
    if (buf->ifd_data_fd)
        close(buf->ifd_data_fd);
    rc = ioctl(ion_fd, ION_IOC_FREE, &handle_data);
    if (rc)
        fprintf(stderr, "ION free failed (%s)\n", strerror(errno));
alloc_fail:
    if (ion_fd >= 0)
        close(ion_fd);
    buf->ion_fd = -1;
    return rc;
}

static int ion_memfree(struct ion_buf_handle *handle)
{
    struct ion_handle_data handle_data;
    int ret;

    ret = munmap(handle->buffer, (handle->buffer_len + 4095) & ~4095);
    if (ret)
        fprintf(stderr, "munmap failed (%s)\n", strerror(errno));

    handle_data.handle = handle->ion_alloc_handle.handle;
    close(handle->ifd_data_fd);
    ret = ioctl(handle->ion_fd, ION_IOC_FREE, &handle_data);
    if (ret)
        fprintf(stderr, "ION free failed (%s)\n", strerror(errno));
    close(handle->ion_fd);

    return ret;
}

static int ion_buffer_clean_inval(struct ion_buf_handle *buf, uint32_t cmd)
{
    struct ion_flush_data data;
    int rc;

    data.fd = buf->ifd_data_fd;
    data.vaddr = buf->buffer;
    data.length = buf->buffer_len;
    data.offset = 0;
    data.handle = buf->ion_alloc_handle.handle;

    rc = ioctl(buf->ion_fd, cmd, &data);
    if (rc < 0)
        fprintf(stderr, "clean_inval cache failed (%s)\n", strerror(errno));

    return rc;
}

static int is_encrypted(int smcmod_fd, struct ion_buf_handle *buf,
                        uint32_t len, uint32_t *ctx_id, uint32_t *end_offset)
{
    struct smcmod_decrypt_req req;
    uint32_t status;
    int ret;

    req.service_id = SCM_SVC_SSD;
    req.command_id = SSD_PARSE_MD_ID;
    req.operation = SMCMOD_DECRYPT_REQ_OP_METADATA;
    req.request.metadata.len =
        (len >= SSD_HEADER_MIN_SIZE) ? SSD_HEADER_MIN_SIZE : len;
    req.request.metadata.ion_fd = buf->ifd_data_fd;

    do {
        ret = ioctl(smcmod_fd, SMCMOD_IOCTL_DECRYPT_CMD, &req);
        if (ret < 0)
            fprintf(stderr, "%s: ioctl ret=%d, %s\n", __func__, ret,
                    strerror(errno));

        status = req.response.metadata.status;

        if (!ret && (status == SSD_PMD_PARSING_INCOMPLETE)) {
            req.request.metadata.len *= MULTIPLICATION_FACTOR;
            continue;
        } else {
            break;
        }
    } while (1);

    if (!ret) {
        if (status == SSD_PMD_ENCRYPTED) {
            *ctx_id = req.response.metadata.ctx_id;
            *end_offset = req.response.metadata.end_offset;
            ret = 1;
        } else {
            fprintf(stderr, "Image is not encrypted (response status %d)\n",
                    status);
        }
    } else {
        fprintf(stderr, "%s: call failed\n", __func__);
    }

    return ret;
}

static int decrypt(int smcmod_fd, struct ion_buf_handle *buf, uint32_t len,
                   uint32_t md_ctx, uint32_t offset)
{
    struct smcmod_decrypt_req req;
    int ret;

    req.service_id = SCM_SVC_SSD;
    req.command_id = SSD_DECRYPT_IMG_FRAG_ID;
    req.operation = SMCMOD_DECRYPT_REQ_OP_IMG_FRAG;
    req.request.img_frag.ctx_id = md_ctx;
    req.request.img_frag.last_frag = 1;
    req.request.img_frag.ion_fd = buf->ifd_data_fd;
    req.request.img_frag.frag_len = len - offset;
    req.request.img_frag.offset = offset;

    ret = ioctl(smcmod_fd, SMCMOD_IOCTL_DECRYPT_CMD, &req);
    if (ret < 0) {
        fprintf(stderr, "decrypt ioctl failed (%s)\n", strerror(errno));
        return ret;
    }

    return 0;
}

static int save_file(const char *fname, unsigned char *buf, size_t len)
{
    FILE *file;
    size_t written;

    file = fopen(fname, "wb");
    if (!file) {
        fprintf(stderr, "Failed to open %s (%s)\n", fname, strerror(errno));
        return -errno;
    }

    written = fwrite(buf, len, 1, file);
    if (written != 1) {
        fclose(file);
        fprintf(stderr, "Failed to write %s (%s)\n", fname, strerror(errno));
        return -errno;
    }
    fflush(file);
    fclose(file);
    fprintf(stdout, "%s written %d bytes\n", fname, len);
    return 0;
}

int decrypt_image(const char *src_file, const char *dst_file)
{
    int ret = -1;
    uint32_t md_ctx = 0, offset = 0;
    uint32_t fsize = 0;
    FILE *file = NULL;
    struct ion_buf_handle ionbuf;
    int smcmod_fd = -1;
    int qseecom_fd = -1;
    size_t read;

    memset(&ionbuf, 0, sizeof(ionbuf));
    ionbuf.ion_fd = -1;

    qseecom_fd = open("/dev/qseecom", O_RDWR);
    if (qseecom_fd < 0) {
        fprintf(stderr, "Failed to open /dev/qseecom device (%s)\n",
                strerror(errno));
        goto exit;
    }

    smcmod_fd = open("/dev/smcmod", O_RDWR);
    if (smcmod_fd < 0) {
        fprintf(stderr, "Failed to open /dev/smcmod device (%s)\n",
                strerror(errno));
        goto exit;
    }

    file = fopen(src_file, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open %s (%s)\n", src_file, strerror(errno));
        goto exit;
    }

    fseek(file, 0, SEEK_END);
    fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    ret = ion_memalloc(&ionbuf, fsize, ION_PIL1_HEAP_ID);
    if (ret)
        goto exit;

    read = fread(ionbuf.buffer, fsize, 1, file);
    if (read != 1) {
        fprintf(stderr, "Failed to read %s (%s)\n", src_file, strerror(errno));
        ret = -errno;
        goto exit;
    }

    ret = ion_buffer_clean_inval(&ionbuf, ION_IOC_CLEAN_INV_CACHES);
    if (ret < 0)
        goto exit;

    ret = ioctl(qseecom_fd, QSEECOM_IOCTL_PERF_ENABLE_REQ);
    if (ret < 0)
        goto exit;

    ret = is_encrypted(smcmod_fd, &ionbuf, fsize, &md_ctx, &offset);
    if (ret < 0)
        goto exit;

    if (ret == 1) {
        fprintf(stdout, "decrypting %s ...\n", src_file);
        ret = decrypt(smcmod_fd, &ionbuf, fsize, md_ctx, offset);
        if (ret < 0)
            goto exit;

        ion_buffer_clean_inval(&ionbuf, ION_IOC_INV_CACHES);

        ret = save_file(dst_file, ionbuf.buffer + offset, fsize - offset);
        if (ret < 0)
            goto exit;

        fprintf(stdout, "decrypting done!\n");
    }

exit:
    if (ionbuf.ion_fd >= 0)
        ion_memfree(&ionbuf);

    if (qseecom_fd >= 0) {
        ioctl(qseecom_fd, QSEECOM_IOCTL_PERF_DISABLE_REQ);
        close(qseecom_fd);
    }

    if (smcmod_fd >= 0)
        close(smcmod_fd);

    if (file)
        fclose(file);

    return ret;
}
