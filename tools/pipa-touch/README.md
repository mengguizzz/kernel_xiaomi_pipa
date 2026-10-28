# pipa Novatek reset DTBO

The working reset path requires the pipa overlay to expose GPIO33 through the
`tlmm` GPIO controller. Rebuilding the full pipa overlay from this source tree
changes unrelated DTBO entries and can panic this 4.19 kernel.

`pipa_dtbo_reset.py` instead modifies only the pipa entry in an already working
DTBO. It requires `fdtput` and `fdtget` from `device-tree-compiler`.

The tested source image is:

```text
SHA256 C7D0D96ED1CE55989D727B71E14A373B8CD8D71BD303758C886E564D0C0D2800
```

Generate the DTBO:

```sh
python3 tools/pipa-touch/pipa_dtbo_reset.py \
  --base dtbo.img \
  --output dtbo-touch-reset.img
```

The default phandle `0x65` is the `tlmm` phandle in the merged pipa device tree
used by this image. Verify it if the base DTBO changes:

```sh
# Pull /sys/firmware/fdt first, then:
fdtget -t x running-fdt.dtb /soc/pinctrl@f000000 phandle
```

The resulting property is:

```dts
novatek,reset-gpio = <0x65 0x21 0x00>;
```

Where `0x21` is GPIO33. The driver enable/resume implementation follows the
e404 `hos4-nabu-pen` touch link and is required for this DTBO property.

## Verification

This implementation was tested across multiple reboots on pipa. No touch loss
was observed during boot. A few rare failures still occurred after screen-off;
in those cases, turning the screen back on recovered touch. This is better than
the previous behavior, where touch could be absent immediately after boot.
