set pagination off
set print pretty off
set print elements 0
set width 0

printf "\n============================================================\n"
printf "AM1802 TinyUSB bootloader dump\n"
printf "============================================================\n"
printf "Decision ladder:\n"
printf "  PHYCLKGD/soft-connect -> wrapper IRQ -> bus reset -> SETUP\n"
printf "  -> stock TinyUSB dispatch -> descriptor callback -> EP0 IN xfer\n"
printf "  -> SET_ADDRESS/config/string progress.\n"
printf "Note: raw MUSB interrupt status registers are read-to-clear, so this\n"
printf "script avoids reading them directly and uses latched counters instead.\n"

printf "\n============================================================\n"
printf "1. Non-clearing raw hardware snapshot\n"
printf "============================================================\n"
printf "CFGCHIP2 / USB0 PHY control @ 0x01c14184 = "
p/x *(volatile unsigned int *)0x01c14184
printf "  key bits: PHYCLKGD=bit17, OTGMODE=bits14:13, PHYCLKMUX=bit11,\n"
printf "            PHYPWRDN=bit10, OTGPWRDN=bit9, PLLON=bit6, REFFREQ=bits3:0\n"

printf "USB0 wrapper INTR_SRC @ 0x01e00020 = "
p/x *(volatile unsigned int *)0x01e00020
printf "USB0 wrapper INTR_MASK @ 0x01e0002c = "
p/x *(volatile unsigned int *)0x01e0002c
printf "USB0 wrapper INTR_SRC_MASKED @ 0x01e00038 = "
p/x *(volatile unsigned int *)0x01e00038
printf "USB0 wrapper END_OF_INTR @ 0x01e0003c = "
p/x *(volatile unsigned int *)0x01e0003c

printf "AINTC GER @ 0xfffee010 = "
p/x *(volatile unsigned int *)0xfffee010
printf "AINTC HIER @ 0xfffef500 = "
p/x *(volatile unsigned int *)0xfffef500
printf "AINTC SRSR1 raw pending @ 0xfffee204 = "
p/x *(volatile unsigned int *)0xfffee204
printf "AINTC SECR1 enabled pending @ 0xfffee284 = "
p/x *(volatile unsigned int *)0xfffee284
printf "  USB0 is SYS_INT_USB0=58, so it is bit 26 in SRSR1/SECR1.\n"

printf "MUSB FADDR @ 0x01e00400 = "
p/x *(volatile unsigned char *)0x01e00400
printf "MUSB POWER @ 0x01e00401 = "
p/x *(volatile unsigned char *)0x01e00401
printf "MUSB TXIE @ 0x01e00406 = "
p/x *(volatile unsigned short *)0x01e00406
printf "MUSB RXIE @ 0x01e00408 = "
p/x *(volatile unsigned short *)0x01e00408
printf "MUSB IE @ 0x01e0040b = "
p/x *(volatile unsigned char *)0x01e0040b
printf "MUSB EPIDX @ 0x01e0040e = "
p/x *(volatile unsigned char *)0x01e0040e
printf "MUSB DEVCTL @ 0x01e00460 = "
p/x *(volatile unsigned char *)0x01e00460
printf "MUSB CSR0L @ 0x01e00502 = "
p/x *(volatile unsigned char *)0x01e00502
printf "MUSB COUNT0 @ 0x01e00508 = "
p/x *(volatile unsigned short *)0x01e00508
printf "  CSR0L bits: RXRDY=1 TXRDY=2 STALLED=4 DATAEND/SETUP=8 SETEND=0x10\n"

printf "\n============================================================\n"
printf "2. AM1802 platform glue: PHY, wrapper IRQ\n"
printf "============================================================\n"
printf "musb_am1802_phy_ctrl_before = "
p/x musb_am1802_phy_ctrl_before
printf "musb_am1802_phy_ctrl_reset = "
p/x musb_am1802_phy_ctrl_reset
printf "musb_am1802_phy_ctrl_configured = "
p/x musb_am1802_phy_ctrl_configured
printf "musb_am1802_phy_ctrl_last = "
p/x musb_am1802_phy_ctrl_last
printf "musb_am1802_phy_wait_count = "
p/x musb_am1802_phy_wait_count
printf "musb_am1802_phy_attempt_count = "
p/x musb_am1802_phy_attempt_count
printf "musb_am1802_phy_selected_mux = "
p/x musb_am1802_phy_selected_mux
printf "musb_am1802_phy_fallback_used = "
p/x musb_am1802_phy_fallback_used
printf "musb_am1802_phy_timeout = "
p/x musb_am1802_phy_timeout

printf "musb_am1802_irq_enabled = "
p/x musb_am1802_irq_enabled
printf "musb_am1802_irq_count = "
p/x musb_am1802_irq_count
printf "musb_am1802_irq_bridge_src = "
p/x musb_am1802_irq_bridge_src
printf "musb_am1802_irq_bridge_masked = "
p/x musb_am1802_irq_bridge_masked
printf "musb_am1802_irq_intr_usb = "
p/x musb_am1802_irq_intr_usb
printf "musb_am1802_irq_intr_tx = "
p/x musb_am1802_irq_intr_tx
printf "musb_am1802_irq_intr_rx = "
p/x musb_am1802_irq_intr_rx
printf "musb_am1802_irq_intr_usben = "
p/x musb_am1802_irq_intr_usben
printf "musb_am1802_irq_intr_txen = "
p/x musb_am1802_irq_intr_txen
printf "musb_am1802_irq_intr_rxen = "
p/x musb_am1802_irq_intr_rxen
printf "musb_am1802_irq_power = "
p/x musb_am1802_irq_power
printf "musb_am1802_irq_devctl = "
p/x musb_am1802_irq_devctl

printf "\n============================================================\n"
printf "3. MUSB DCD: bus reset and latest SETUP packet\n"
printf "============================================================\n"
printf "musb_dcd_bus_reset_count = "
p/x musb_dcd_bus_reset_count
printf "musb_dcd_setup_count = "
p/x musb_dcd_setup_count
printf "musb_dcd_ignored_sof_count = "
p/x musb_dcd_ignored_sof_count

printf "musb_dcd_last_setup_raw0 = "
p/x musb_dcd_last_setup_raw0
printf "musb_dcd_last_setup_raw1 = "
p/x musb_dcd_last_setup_raw1
printf "musb_dcd_last_setup0 = "
p/x musb_dcd_last_setup0
printf "musb_dcd_last_setup1 = "
p/x musb_dcd_last_setup1
printf "musb_dcd_last_setup_bm_request_type = "
p/x musb_dcd_last_setup_bm_request_type
printf "musb_dcd_last_setup_b_request = "
p/x musb_dcd_last_setup_b_request
printf "musb_dcd_last_setup_w_value = "
p/x musb_dcd_last_setup_w_value
printf "musb_dcd_last_setup_w_index = "
p/x musb_dcd_last_setup_w_index
printf "musb_dcd_last_setup_w_length = "
p/x musb_dcd_last_setup_w_length

printf "musb_dcd_last_setup_bytes = "
p/x musb_dcd_last_setup_byte0
p/x musb_dcd_last_setup_byte1
p/x musb_dcd_last_setup_byte2
p/x musb_dcd_last_setup_byte3
p/x musb_dcd_last_setup_byte4
p/x musb_dcd_last_setup_byte5
p/x musb_dcd_last_setup_byte6
p/x musb_dcd_last_setup_byte7
printf "  GET_DESCRIPTOR device should be bm=0x80 bRequest=0x06 wValue=0x0100.\n"

printf "musb_dcd_event_handoff_magic = "
p/x musb_dcd_event_handoff_magic
printf "  Expected 0xe0068006 for GET_DESCRIPTOR(Device): event=0x06 bm=0x80 req=0x06.\n"
printf "musb_dcd_event_size = "
p/x musb_dcd_event_size
printf "musb_dcd_event_setup_offset = "
p/x musb_dcd_event_setup_offset
printf "  Expected event_size=0x0c setup_offset=0x04.\n"
printf "musb_dcd_event_setup_count = "
p/x musb_dcd_event_setup_count
printf "musb_dcd_event_setup_bm_request_type = "
p/x musb_dcd_event_setup_bm_request_type
printf "musb_dcd_event_setup_b_request = "
p/x musb_dcd_event_setup_b_request
printf "musb_dcd_event_setup_w_value = "
p/x musb_dcd_event_setup_w_value
printf "musb_dcd_event_setup_w_index = "
p/x musb_dcd_event_setup_w_index
printf "musb_dcd_event_setup_w_length = "
p/x musb_dcd_event_setup_w_length
printf "musb_dcd_event_setup_bytes = "
p/x musb_dcd_event_setup_byte0
p/x musb_dcd_event_setup_byte1
p/x musb_dcd_event_setup_byte2
p/x musb_dcd_event_setup_byte3
p/x musb_dcd_event_setup_byte4
p/x musb_dcd_event_setup_byte5
p/x musb_dcd_event_setup_byte6
p/x musb_dcd_event_setup_byte7
printf "  These are the bytes in dcd_event_t just before dcd_event_handler().\n"
printf "musb_dcd_event_raw_words = "
p/x musb_dcd_event_raw0
p/x musb_dcd_event_raw1
p/x musb_dcd_event_raw2
printf "  For current SETUP, raw2 packs wIndex/wLength. Device descriptor retries may be 0x00120000; first 64-byte requests are 0x00400000.\n"

printf "\n============================================================\n"
printf "4. TinyUSB stock queue state\n"
printf "============================================================\n"
printf "No usbd.c instrumentation is used here; TinyUSB core is left stock.\n"
printf "_usbd_qdef.item_size = "
p/x _usbd_qdef.item_size
printf "_usbd_qdef.ff.depth = "
p/x _usbd_qdef.ff.depth
printf "_usbd_qdef.ff.wr_idx = "
p/x _usbd_qdef.ff.wr_idx
printf "_usbd_qdef.ff.rd_idx = "
p/x _usbd_qdef.ff.rd_idx
printf "_usbd_qdef.ff.wr_idx %% item_size = "
p/x _usbd_qdef.ff.wr_idx % _usbd_qdef.item_size
printf "_usbd_qdef.ff.rd_idx %% item_size = "
p/x _usbd_qdef.ff.rd_idx % _usbd_qdef.item_size
printf "  Queue idx mod item_size should stay 0; nonzero means event byte-boundary drift.\n"

printf "\n============================================================\n"
printf "5. Bootloader descriptor callbacks\n"
printf "============================================================\n"
printf "boot_usb_descriptor_device_count = "
p/x boot_usb_descriptor_device_count
printf "boot_usb_descriptor_configuration_count = "
p/x boot_usb_descriptor_configuration_count
printf "boot_usb_descriptor_configuration_last_index = "
p/x boot_usb_descriptor_configuration_last_index
printf "boot_usb_descriptor_configuration_last_speed = "
p/x boot_usb_descriptor_configuration_last_speed
printf "boot_usb_descriptor_configuration_last_len = "
p/x boot_usb_descriptor_configuration_last_len
printf "boot_usb_descriptor_string_count = "
p/x boot_usb_descriptor_string_count
printf "boot_usb_descriptor_string_last_index = "
p/x boot_usb_descriptor_string_last_index
printf "boot_usb_descriptor_string_last_langid = "
p/x boot_usb_descriptor_string_last_langid
printf "boot_usb_descriptor_string_last_length = "
p/x boot_usb_descriptor_string_last_length
printf "boot_usb_descriptor_string_invalid_count = "
p/x boot_usb_descriptor_string_invalid_count

printf "\n============================================================\n"
printf "6. EP0 transfer state machine\n"
printf "============================================================\n"
printf "musb_dcd_ep0_isr_count = "
p/x musb_dcd_ep0_isr_count
printf "musb_dcd_ep0_isr_last_csrl = "
p/x musb_dcd_ep0_isr_last_csrl
printf "musb_dcd_ep0_isr_last_count0 = "
p/x musb_dcd_ep0_isr_last_count0
printf "musb_dcd_ep0_isr_last_state = "
p/x musb_dcd_ep0_isr_last_state

printf "musb_dcd_ep0_xfer_count = "
p/x musb_dcd_ep0_xfer_count
printf "musb_dcd_ep0_xfer_last_ep_addr = "
p/x musb_dcd_ep0_xfer_last_ep_addr
printf "musb_dcd_ep0_xfer_last_len = "
p/x musb_dcd_ep0_xfer_last_len
printf "musb_dcd_ep0_xfer_last_state_before = "
p/x musb_dcd_ep0_xfer_last_state_before
printf "musb_dcd_ep0_xfer_last_state_after = "
p/x musb_dcd_ep0_xfer_last_state_after
printf "musb_dcd_ep0_xfer_last_result = "
p/x musb_dcd_ep0_xfer_last_result
printf "musb_dcd_ep0_xfer_last_csr0l = "
p/x musb_dcd_ep0_xfer_last_csr0l
printf "musb_dcd_ep0_xfer_last_remain_wlength = "
p/x musb_dcd_ep0_xfer_last_remain_wlength

printf "musb_dcd_ep0_process_count = "
p/x musb_dcd_ep0_process_count
printf "musb_dcd_ep0_process_state_before = "
p/x musb_dcd_ep0_process_state_before
printf "musb_dcd_ep0_process_state_after = "
p/x musb_dcd_ep0_process_state_after
printf "musb_dcd_ep0_state_current = "
p/x musb_dcd_ep0_state_current

printf "musb_dcd_ep0_dataend_ignored_count = "
p/x musb_dcd_ep0_dataend_ignored_count
printf "musb_dcd_ep0_dataend_processed_count = "
p/x musb_dcd_ep0_dataend_processed_count
printf "musb_dcd_ep0_status_out_zlp_count = "
p/x musb_dcd_ep0_status_out_zlp_count
printf "musb_dcd_ep0_status_out_auto_complete_count = "
p/x musb_dcd_ep0_status_out_auto_complete_count
printf "musb_dcd_ep0_status_out_auto_xfer_count = "
p/x musb_dcd_ep0_status_out_auto_xfer_count
printf "musb_dcd_ep0_stalled_count = "
p/x musb_dcd_ep0_stalled_count
printf "musb_dcd_ep0_setupend_count = "
p/x musb_dcd_ep0_setupend_count
printf "musb_dcd_ep0_stall_request_count = "
p/x musb_dcd_ep0_stall_request_count
printf "musb_dcd_ep0_stall_last_ep_addr = "
p/x musb_dcd_ep0_stall_last_ep_addr
printf "musb_dcd_ep0_deferred_setup_count = "
p/x musb_dcd_ep0_deferred_setup_count
printf "musb_dcd_ep0_replayed_setup_count = "
p/x musb_dcd_ep0_replayed_setup_count

printf "\n============================================================\n"
printf "7. Address progress\n"
printf "============================================================\n"
printf "musb_dcd_set_address_count = "
p/x musb_dcd_set_address_count
printf "musb_dcd_set_address_last = "
p/x musb_dcd_set_address_last
printf "musb_dcd_address_applied_count = "
p/x musb_dcd_address_applied_count
printf "musb_dcd_faddr_last = "
p/x musb_dcd_faddr_last

printf "\n============================================================\n"
printf "8. MUSB non-control endpoint path\n"
printf "============================================================\n"
printf "musb_dcd_edpt_open_count = "
p/x musb_dcd_edpt_open_count
printf "musb_dcd_edpt_open_last_ep_addr = "
p/x musb_dcd_edpt_open_last_ep_addr
printf "musb_dcd_edpt_open_last_mps = "
p/x musb_dcd_edpt_open_last_mps
printf "musb_dcd_edpt_open_last_xfer = "
p/x musb_dcd_edpt_open_last_xfer
printf "musb_dcd_edpt_open_last_is_rx = "
p/x musb_dcd_edpt_open_last_is_rx
printf "musb_dcd_edpt_open_last_csrh = "
p/x musb_dcd_edpt_open_last_csrh
printf "musb_dcd_edpt_open_last_double_packet = "
p/x musb_dcd_edpt_open_last_double_packet
printf "musb_dcd_edpt_open_last_intr_txen = "
p/x musb_dcd_edpt_open_last_intr_txen
printf "musb_dcd_edpt_open_last_intr_rxen = "
p/x musb_dcd_edpt_open_last_intr_rxen
printf "musb_dcd_edpt_xfer_count = "
p/x musb_dcd_edpt_xfer_count
printf "musb_dcd_edpt_xfer_last_ep_addr = "
p/x musb_dcd_edpt_xfer_last_ep_addr
printf "musb_dcd_edpt_xfer_last_len = "
p/x musb_dcd_edpt_xfer_last_len
printf "musb_dcd_edpt_xfer_last_is_fifo = "
p/x musb_dcd_edpt_xfer_last_is_fifo
printf "musb_dcd_edpt_xfer_last_result = "
p/x musb_dcd_edpt_xfer_last_result
printf "musb_dcd_epout_isr_count = "
p/x musb_dcd_epout_isr_count
printf "musb_dcd_epout_last_epnum = "
p/x musb_dcd_epout_last_epnum
printf "musb_dcd_epout_last_csrl = "
p/x musb_dcd_epout_last_csrl
printf "musb_dcd_epout_last_rx_count = "
p/x musb_dcd_epout_last_rx_count
printf "musb_dcd_epout_last_packet_offset = "
p/x musb_dcd_epout_last_packet_offset
printf "musb_dcd_epout_last_word0 = "
p/x musb_dcd_epout_last_word0
printf "musb_dcd_epout_last_word1 = "
p/x musb_dcd_epout_last_word1
printf "  CBW starts with little-endian word0=0x43425355 ('USBC' in memory).\n"
printf "musb_dcd_epout_last_xferred = "
p/x musb_dcd_epout_last_xferred
printf "musb_dcd_epout_complete_count = "
p/x musb_dcd_epout_complete_count
printf "musb_dcd_epout_no_buffer_count = "
p/x musb_dcd_epout_no_buffer_count
printf "musb_dcd_epout_no_rxrdy_count = "
p/x musb_dcd_epout_no_rxrdy_count
printf "musb_dcd_epout_stalled_count = "
p/x musb_dcd_epout_stalled_count
printf "musb_dcd_epin_isr_count = "
p/x musb_dcd_epin_isr_count
printf "musb_dcd_epin_last_write_len = "
p/x musb_dcd_epin_last_write_len
printf "musb_dcd_epin_last_write_word0 = "
p/x musb_dcd_epin_last_write_word0
printf "musb_dcd_epin_last_write_word1 = "
p/x musb_dcd_epin_last_write_word1
printf "musb_dcd_epin_last_epnum = "
p/x musb_dcd_epin_last_epnum
printf "musb_dcd_epin_last_csrl = "
p/x musb_dcd_epin_last_csrl
printf "musb_dcd_epin_last_xferred = "
p/x musb_dcd_epin_last_xferred
printf "musb_dcd_epin_complete_count = "
p/x musb_dcd_epin_complete_count
printf "musb_dcd_epin_fifone_wait_count = "
p/x musb_dcd_epin_fifone_wait_count
printf "musb_dcd_epin_stalled_count = "
p/x musb_dcd_epin_stalled_count
printf "  EP opens should reach 2 for MSC OUT/IN. Bulk CBW arrival increments epout_complete.\n"
printf "  SCSI response/status traffic should increment epin_complete.\n"

printf "\n============================================================\n"
printf "9. Bootloader MSC RAM disk\n"
printf "============================================================\n"
printf "boot_msc_get_maxlun_count = "
p/x boot_msc_get_maxlun_count
printf "boot_msc_inquiry_count = "
p/x boot_msc_inquiry_count
printf "boot_msc_test_ready_count = "
p/x boot_msc_test_ready_count
printf "boot_msc_capacity_count = "
p/x boot_msc_capacity_count
printf "boot_msc_start_stop_count = "
p/x boot_msc_start_stop_count
printf "boot_msc_scsi_count = "
p/x boot_msc_scsi_count
printf "boot_msc_scsi_supported_count = "
p/x boot_msc_scsi_supported_count
printf "boot_msc_scsi_unsupported_count = "
p/x boot_msc_scsi_unsupported_count
printf "boot_msc_scsi_last_result = "
p/x boot_msc_scsi_last_result
printf "boot_msc_prevent_allow_count = "
p/x boot_msc_prevent_allow_count
printf "boot_msc_request_sense_count = "
p/x boot_msc_request_sense_count
printf "boot_msc_read10_complete_count = "
p/x boot_msc_read10_complete_count
printf "boot_msc_write10_complete_count = "
p/x boot_msc_write10_complete_count
printf "boot_msc_scsi_complete_count = "
p/x boot_msc_scsi_complete_count
printf "boot_msc_last_lun = "
p/x boot_msc_last_lun
printf "boot_msc_last_scsi_cmd = "
p/x boot_msc_last_scsi_cmd
printf "boot_msc_last_prevent = "
p/x boot_msc_last_prevent
printf "boot_msc_last_control = "
p/x boot_msc_last_control
printf "boot_msc_ejected = "
p/x boot_msc_ejected
printf "boot_msc_bpb_bytes_per_sector = "
p/x boot_msc_bpb_bytes_per_sector
printf "boot_msc_bpb_sectors_per_cluster = "
p/x boot_msc_bpb_sectors_per_cluster
printf "boot_msc_bpb_total_sectors = "
p/x boot_msc_bpb_total_sectors
printf "boot_msc_bpb_signature = "
p/x boot_msc_bpb_signature
printf "  BPB should be bytes_per_sector=0x200 sectors_per_cluster=1 total=0x10 signature=0xaa55.\n"
printf "boot_msc_read10_count = "
p/x boot_msc_read10_count
printf "boot_msc_write10_count = "
p/x boot_msc_write10_count
printf "boot_msc_last_read_lba = "
p/x boot_msc_last_read_lba
printf "boot_msc_last_write_lba = "
p/x boot_msc_last_write_lba
printf "boot_msc_last_write_offset = "
p/x boot_msc_last_write_offset
printf "boot_msc_last_write_size = "
p/x boot_msc_last_write_size
printf "boot_msc_scan_count = "
p/x boot_msc_scan_count
printf "boot_msc_scan_entry_count = "
p/x boot_msc_scan_entry_count
printf "boot_msc_scan_empty_count = "
p/x boot_msc_scan_empty_count
printf "boot_msc_scan_skip_attr_count = "
p/x boot_msc_scan_skip_attr_count
printf "boot_msc_scan_skip_invalid_count = "
p/x boot_msc_scan_skip_invalid_count
printf "boot_msc_scan_skip_readme_count = "
p/x boot_msc_scan_skip_readme_count
printf "boot_msc_scan_skip_unchanged_count = "
p/x boot_msc_scan_skip_unchanged_count
printf "boot_msc_scan_last_entry_index = "
p/x boot_msc_scan_last_entry_index
printf "boot_msc_scan_last_attr = "
p/x boot_msc_scan_last_attr
printf "boot_msc_scan_last_cluster = "
p/x boot_msc_scan_last_cluster
printf "boot_msc_file_log_count = "
p/x boot_msc_file_log_count
printf "boot_msc_last_file_size = "
p/x boot_msc_last_file_size
printf "boot_msc_last_file_checksum = "
p/x boot_msc_last_file_checksum
printf "  read/write counts prove Windows reached MSC bulk callbacks.\n"
printf "  file_log_count increments when a normal FAT12 root file changes.\n"

printf "\n============================================================\n"
printf "Quick read guide\n"
printf "============================================================\n"
printf "No PHYCLKGD or musb_am1802_phy_timeout=1: clock/PHY configuration.\n"
printf "PHY good but musb_am1802_irq_count=0: wrapper/AINTC interrupt path.\n"
printf "IRQ good but musb_dcd_bus_reset_count=0: reset interrupt decode/connect.\n"
printf "Bus reset good but musb_dcd_setup_count=0: EP0 SETUP IRQ/FIFO path.\n"
printf "MUSB handoff good but descriptor count=0: stock TinyUSB dispatch not reaching callbacks.\n"
printf "Descriptor count good but EP0 xfer len not 0x12 for device descriptor: response setup.\n"
printf "EP0 xfer good but still descriptor failure: EP0 FIFO/TXRDY/DATAEND/status timing.\n"
printf "============================================================\n"
