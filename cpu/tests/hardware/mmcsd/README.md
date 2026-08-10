# AM1802 MMC/SD hardware test

This target is an isolated, read-only test image for developing
`cpu/src/kernel/peripheral/per_mmcsd_ft.c` without changing the MMC/SD driver
used by the kernel or bootloader.

## Build and launch

Use the VS Code task `Build MMCSD hardware test`, then start the
`Debug MMCSD hardware test` launch configuration.

The debugger first runs `debug_bringup` from OC RAM to initialize clocks and
DDR. It then loads `mmcsd_hw_test.elf` at `0xC0000000`, enables semihosting,
and stops at `mmcsd_hw_test_complete` after the test summary is printed.

The equivalent build command is:

```powershell
cmake --build build --target debug_bringup mmcsd_hw_test
```

## Adding tests

Add polling-only test functions to `main.c` and register each one with
`HW_TEST_RUN`. Use `HW_TEST_ASSERT` or `HW_TEST_ASSERT_EQ_U32` to report
failures through the J-Link semihosting console.

Keep this suite read-only until a separate destructive-test policy and a
disposable sector range are defined. Do not link `dev_sdcard`, FatFS, or the
legacy `per_mmcsd.c` into this target.
