






















c_SRC_FILES += audio/framework/plugs/source/a2dp_file.c audio/framework/plugs/source/a2dp_streamctrl.c audio/framework/plugs/source/esco_file.c audio/framework/plugs/source/adc_file.c audio/framework/nodes/esco_tx_node.c audio/framework/nodes/plc_node.c audio/framework/nodes/volume_node.c audio/framework/node_list.c
c_SRC_FILES += audio/framework/nodes/cvp_dms_node.c
c_SRC_FILES += audio/framework/nodes/cvp_develop_node.c
c_SRC_FILES += audio/test_tools/audio_dut_control.c audio/test_tools/audio_dut_control_old.c





c_SRC_FILES += audio/common/audio_node_config.c audio/common/audio_dvol.c audio/common/audio_general.c audio/common/audio_build_needed.c audio/common/online_debug/aud_data_export.c audio/common/online_debug/audio_online_debug.c audio/common/online_debug/audio_capture.c audio/common/audio_plc.c audio/common/audio_noise_gate.c audio/common/audio_ns.c audio/common/audio_utils.c audio/common/audio_export_demo.c audio/common/amplitude_statistic.c audio/common/frame_length_adaptive.c audio/common/bt_audio_energy_detection.c audio/common/audio_event_handler.c audio/common/debug/audio_debug.c audio/common/power/mic_power_manager.c audio/common/audio_volume_mixer.c audio/common/audio_effect_verify.c audio/common/pcm_data/sine_pcm.c
c_SRC_FILES += audio/common/uartPcmSender.c



c_SRC_FILES += audio/common/demo/audio_demo.c
c_SRC_FILES += audio/common/demo/hw_math_v2_demo.c
c_SRC_FILES += audio/interface/player/tone_player.c audio/interface/player/ring_player.c audio/interface/player/a2dp_player.c audio/interface/player/esco_player.c audio/interface/player/key_tone_player.c audio/interface/player/dev_flow_player.c audio/interface/player/adda_loop_player.c audio/interface/player/ai_rx_player.c
c_SRC_FILES += audio/interface/recoder/esco_recoder.c audio/interface/recoder/dev_flow_recoder.c
c_SRC_FILES += audio/interface/user_defined/audio_dsp_low_latency_player.c audio/interface/user_defined/env_noise_recoder.c
c_SRC_FILES += audio/effect/eq_config.c audio/effect/spk_eq.c audio/effect/audio_voice_changer_api.c audio/effect/esco_ul_voice_changer.c audio/effect/bass_treble.c audio/effect/audio_dc_offset_remove.c audio/effect/effects_adj.c audio/effect/effects_dev.c audio/effect/effects_default_param.c audio/effect/node_param_update.c audio/effect/scene_update.c
c_SRC_FILES += audio/framework/nodes/spatial_effects_node.c audio/common/online_debug/aud_spatial_effect_dut.c



c_SRC_FILES += audio/effect/spatial_effect/spatial_effect.c audio/effect/spatial_effect/spatial_effect_imu.c audio/effect/spatial_effect/spatial_effect_tws.c audio/effect/spatial_effect/spatial_imu_trim.c audio/effect/spatial_effect/spatial_effects_process.c
c_SRC_FILES += audio/CVP/audio_aec.c audio/CVP/audio_cvp.c audio/CVP/audio_cvp_sms_vf.c audio/CVP/audio_cvp_dms.c audio/CVP/audio_cvp_3mic.c audio/CVP/audio_cvp_v3.c audio/CVP/audio_cvp_online.c audio/CVP/audio_cvp_demo.c audio/CVP/audio_cvp_develop.c audio/CVP/audio_cvp_sync.c audio/CVP/audio_cvp_ais_3mic.c audio/CVP/audio_cvp_ref_task.c audio/CVP/audio_cvp_config.c audio/CVP/audio_cvp_elevoc.c
c_SRC_FILES += audio/interface/player/tws_tone_player.c
c_SRC_FILES += audio/framework/plugs/source/linein_file.c
c_SRC_FILES += audio/cpu/common.c
c_SRC_FILES += apps/common/lib_version/version_check.c apps/common/config/bt_profile_config.c



c_SRC_FILES += apps/common/lib_log_config/btctrler_log_config.c apps/common/lib_log_config/btstack_log_config.c apps/common/lib_log_config/driver_log_config.c apps/common/lib_log_config/media_log_config.c apps/common/lib_log_config/net_log_config.c apps/common/lib_log_config/system_log_config.c apps/common/lib_log_config/update_log_config.c
c_SRC_FILES += apps/common/config/new_cfg_tool.c apps/common/config/cfg_tool_statistics_functions/cfg_tool_statistics.c
c_SRC_FILES += apps/common/config/app_config.c



c_SRC_FILES += apps/common/config/ci_transport_uart.c
c_SRC_FILES += apps/common/debug/memory_debug.c
c_SRC_FILES += apps/common/update/update.c



c_SRC_FILES += apps/common/update/testbox_update.c




c_SRC_FILES += apps/common/update/testbox_uart_update.c
c_SRC_FILES += apps/common/ui/pwm_led/led_ui_api.c apps/common/ui/pwm_led/led_ui_tws_sync.c
c_SRC_FILES += apps/common/third_party_profile/common/3th_profile_api.c apps/common/third_party_profile/multi_protocol_main.c




c_SRC_FILES += apps/common/third_party_profile/multi_protocol_common.c apps/common/third_party_profile/multi_protocol_event.c
c_SRC_FILES += apps/common/third_party_profile/custom_protocol_demo/custom_protocol.c
c_SRC_FILES += apps/common/third_party_profile/jieli/online_db/spp_online_db.c apps/common/third_party_profile/jieli/online_db/online_db_deal.c
c_SRC_FILES += apps/common/jldtp/uart_transport.c apps/common/jldtp/jldtp_manager.c
c_SRC_FILES += apps/common/device/key/key_driver.c



c_SRC_FILES += apps/common/device/key/iokey.c
c_SRC_FILES += apps/common/device/usb/device/usb_pll_trim.c
c_SRC_FILES += apps/common/device/usb/device/msd_upgrade.c
c_SRC_FILES += cpu/components/iic_soft.c cpu/components/iic_api.c cpu/components/ir_encoder.c cpu/components/ir_decoder.c cpu/components/rdec_soft.c







c_SRC_FILES += cpu/components/led_api.c cpu/components/two_io_led.c
c_SRC_FILES += cpu/config/gpio_file_parse.c cpu/config/lib_power_config.c
c_SRC_FILES += audio/cpu/br50/audio_setup.c audio/cpu/br50/audio_dai/audio_pdm.c audio/cpu/br50/audio_config.c audio/cpu/br50/audio_pmu.c audio/cpu/br50/audio_configs_dump.c
c_SRC_FILES += audio/cpu/br50/audio_accelerator/hw_fft.c


c_SRC_FILES += audio/cpu/br50/audio_demo/audio_adc_demo.c
c_SRC_FILES += cpu/br50/setup.c cpu/br50/overlay_code.c





c_SRC_FILES += cpu/br50/charge/charge.c




c_SRC_FILES += cpu/br50/charge/charge_config.c
c_SRC_FILES += cpu/br50/charge/chargestore.c cpu/br50/charge/chargestore_config.c
c_SRC_FILES += cpu/components/pwm_led_v1.c




c_SRC_FILES += cpu/br50/power/key_wakeup.c cpu/br50/power/power_app.c cpu/br50/power/power_config.c cpu/br50/power/power_port.c
