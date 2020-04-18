#
# vendor props for mata
#


# Audio
PRODUCT_PROPERTY_OVERRIDES += \
    af.fast_track_multiplier=1 \
    persist.vendor.audio.fluence.audiorec=false \
    persist.vendor.audio.fluence.speaker=true \
    persist.vendor.audio.fluence.voicecall=true \
    persist.vendor.audio.fluence.voicerec=false \
    persist.vendor.audio.fluence.voicecomm=true \
    persist.vendor.audio.hw.binder.size_kbyte=1024 \
    persist.vendor.audio.ras.enabled=false \
    ro.config.media_vol_steps=25 \
    ro.config.vc_call_vol_steps=7 \
    ro.vendor.audio.sdk.fluencetype=fluence \
    ro.vendor.audio.sdk.ssr=false \
    vendor.audio_hal.period_size=192 \
    vendor.audio.offload.buffer.size.kb=32 \
    vendor.audio.offload.gapless.enabled=true \
    vendor.audio.offload.multiple.enabled=false \
    vendor.audio.offload.passthrough=false \
    vendor.audio.safx.pbe.enabled=true

PRODUCT_PROPERTY_OVERRIDES += \
    audio.offload.min.duration.secs=0 \
    vendor.voice.conc.fallbackpath="" \
    audio.offload.video=true \
    audio.offload.disable=false \
    audio.deep_buffer.media=true \
    vendor.audio.av.streaming.offload.enable=false \
    vendor.audio.offload.track.enable=true \
    vendor.audio.offload.multiple.enabled=false \
    vendor.voice.dsd.playback.conc.disabled=true \
    persist.vendor.audio.sva.conc.enabled=false \
    persist.vendor.audio.va_concurrency_enabled=false \
    vendor.voice.path.for.pcm.voip=true \
    vendor.voice.playback.conc.disabled=false \
    vendor.voice.record.conc.disabled=false \
    vendor.voice.voip.conc.disabled=false

PRODUCT_PROPERTY_OVERRIDES += \
    vendor.audio.feature.a2dp_offload.enable=false \
    vendor.audio.feature.afe_proxy.enable=true \
    vendor.audio.feature.anc_headset.enable=true \
    vendor.audio.feature.battery_listener.enable=false \
    vendor.audio.feature.compr_cap.enable=false \
    vendor.audio.feature.compress_in.enable=false \
    vendor.audio.feature.compress_meta_data.enable=true \
    vendor.audio.feature.compr_voip.enable=true \
    vendor.audio.feature.concurrent_capture.enable=false \
    vendor.audio.feature.custom_stereo.enable=true \
    vendor.audio.feature.display_port.enable=true \
    vendor.audio.feature.dsm_feedback.enable=false \
    vendor.audio.feature.dynamic_ecns.enable=false \
    vendor.audio.feature.ext_hw_plugin.enable=false \
    vendor.audio.feature.external_dsp.enable=false \
    vendor.audio.feature.external_speaker.enable=false \
    vendor.audio.feature.external_speaker_tfa.enable=false \
    vendor.audio.feature.fluence.enable=true \
    vendor.audio.feature.fm.enable=false \
    vendor.audio.feature.hdmi_edid.enable=false \
    vendor.audio.feature.hdmi_passthrough.enable=false \
    vendor.audio.feature.hfp.enable=false \
    vendor.audio.feature.hifi_audio.enable=false \
    vendor.audio.feature.hwdep_cal.enable=false \
    vendor.audio.feature.incall_music.enable=false \
    vendor.audio.feature.multi_voice_session.enable=true \
    vendor.audio.feature.keep_alive.enable=false \
    vendor.audio.feature.kpi_optimize.enable=true \
    vendor.audio.feature.maxx_audio.enable=false \
    vendor.audio.feature.ras.enable=true \
    vendor.audio.feature.record_play_concurency.enable=false \
    vendor.audio.feature.src_trkn.enable=true \
    vendor.audio.feature.spkr_prot.enable=false \
    vendor.audio.feature.ssrec.enable=false \
    vendor.audio.feature.usb_offload.enable=true \
    vendor.audio.feature.usb_offload_burst_mode.enable=false \
    vendor.audio.feature.usb_offload_sidetone_volume.enable=false \
    vendor.audio.feature.deepbuffer_as_primary.enable=false \
    vendor.audio.feature.vbat.enable=true \
    vendor.audio.feature.wsa.enable=false \
    vendor.audio.feature.audiozoom.enable=false \
    vendor.audio.feature.snd_mon.enable=false

# Bluetooth
PRODUCT_PROPERTY_OVERRIDES += \
    persist.vendor.bt.a2dp_offload_cap=sbc-aptx-aptxhd-aac \
    vendor.qcom.bluetooth.soc=cherokee \
    persist.vendor.bluetooth.a4wp=false

# Camera
PRODUCT_PROPERTY_OVERRIDES += \
    persist.camera.expose.aux=1 \
    persist.camera.max.previewfps=60 \
    vidc.enc.dcvs.extra-buff-count=2

# Display
PRODUCT_PROPERTY_OVERRIDES += \
    debug.gralloc.enable_fb_ubwc=1 \
    debug.sf.hw=1 \
    ro.opengles.version=196610 \
    ro.qualcomm.cabl=2 \
    vendor.display.disable_partial_split=1

# Enforce privapp-permissions whitelist
PRODUCT_PROPERTY_OVERRIDES += \
    ro.control_privapp_permissions=enforce

# Factory reset partition
PRODUCT_PROPERTY_OVERRIDES += \
    ro.frp.pst=/dev/block/platform/soc/1da4000.ufshc/by-name/frp

# HDR
PRODUCT_PROPERTY_OVERRIDES += \
    ro.qcom.hdr.config=/vendor/etc/hdr_tm_config.xml

# OEM Unlock reporting
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.oem_unlock_supported=1

# Radio
PRODUCT_PROPERTY_OVERRIDES += \
    persist.cne.feature=1 \
    persist.data.df.dev_name=rmnet_usb0 \
    persist.data.df.iwlan_mux=9 \
    persist.data.iwlan.enable=true \
    persist.data.mode=concurrent \
    persist.data.netmgrd.qos.enable=true \
    persist.radio.VT_CAM_INTERFACE=2 \
    persist.radio.VT_ENABLE=1 \
    persist.radio.VT_HYBRID_ENABLE=1 \
    persist.radio.apm_sim_not_pwdn=1 \
    persist.radio.data_con_rprt=true \
    persist.rcs.supported=1 \
    persist.rmnet.data.enable=true \
    persist.vendor.ims.dropset_feature=0 \
    persist.vendor.radio.all_bc_msg=all \
    persist.vendor.radio.custom_ecc=1 \
    persist.vendor.radio.rat_on=other \
    persist.vendor.radio.sib16_support=1 \
    vendor.rild.libpath=/vendor/lib64/libril-qc-qmi-1.so \
    ril.subscription.types=NV,RUIM \
    ro.telephony.default_network=10 \
    telephony.lteOnCdmaDevice=1

# Sensors
PRODUCT_PROPERTY_OVERRIDES += \
    ro.vendor.sensors.dev_ori=false \
    ro.vendor.sensors.dpc=true \
    ro.vendor.sensors.pmd=true \
    ro.vendor.sensors.mot_detect=true \
    ro.vendor.sensors.multishake=true \
    ro.vendor.sensors.sta_detect=true \
    ro.vendor.sensors.wu=false

# SurfaceFlinger
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
    ro.surface_flinger.force_hwc_copy_for_virtual_displays=true \
    ro.surface_flinger.has_HDR_display=true \
    ro.surface_flinger.max_virtual_display_dimension=4096 \
    ro.surface_flinger.protected_contents=true \
    ro.surface_flinger.vsync_event_phase_offset_ns=2000000 \
    ro.surface_flinger.vsync_sf_event_phase_offset_ns=6000000

PRODUCT_PROPERTY_OVERRIDES += \
    debug.sf.early_phase_offset_ns=1500000 \
    debug.sf.early_app_phase_offset_ns=1500000 \
    debug.sf.early_gl_phase_offset_ns=3000000 \
    debug.sf.early_gl_app_phase_offset_ns=15000000

# Time daemon
PRODUCT_PROPERTY_OVERRIDES += \
    persist.delta_time.enable=true

# USB
PRODUCT_PROPERTY_OVERRIDES += \
    persist.sys.usb.config.extra=none

# Widevine
PRODUCT_PROPERTY_OVERRIDES += \
    drm.service.enabled=true

# Wifi
PRODUCT_PROPERTY_OVERRIDES += \
    wifi.interface=wlan0
