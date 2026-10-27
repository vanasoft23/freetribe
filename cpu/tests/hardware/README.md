# AM1802 MMC/SD hardware test

This target is an isolated hardware-test image.

## Build and launch

Use the VS Code task `Build MMCSD hardware test`, then start the
`Debug MMCSD hardware test` launch configuration.

The debugger first runs `ddr_bringup` from OC RAM to initialize clocks and
DDR. It then loads `hw_test.elf` at `0xC0000000`, enables semihosting, and
stops after the test summary is printed.

The equivalent build command is:

```powershell
cmake --build --preset hw-test
```

## Adding tests

Add polling-only test functions to `main.c` and register each one with
`HW_TEST_RUN`. Use `HW_TEST_ASSERT` or `HW_TEST_ASSERT_EQ_U32` to report
failures through the J-Link semihosting console.

Keep this suite read-only until a separate destructive-test policy and a
disposable sector range are defined. Do not link `dev_sdcard`, FatFS, or the
legacy `per_mmcsd.c` into this target.
