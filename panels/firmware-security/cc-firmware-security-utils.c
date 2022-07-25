/* cc-firmware-security-utils.c
 *
 * Copyright (C) 2021 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Kate Hsuan <hpa@redhat.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include "cc-firmware-security-utils.h"

const gchar *
fu_security_attr_get_name (const gchar *appstream_id)
{
  struct
  {
    const gchar *id;
    const gchar *name;
  } attr_to_name[] = {
    /* TRANSLATORS: Title: SPI refers to the flash chip in the computer */
    { FWUPD_SECURITY_ATTR_ID_SPI_BIOSWE, N_("SPI write"), },
    /* TRANSLATORS: Title: SPI refers to the flash chip in the computer */
    { FWUPD_SECURITY_ATTR_ID_SPI_BLE, N_("SPI lock"), },
    /* TRANSLATORS: Title: SPI refers to the flash chip in the computer */
    { FWUPD_SECURITY_ATTR_ID_SPI_SMM_BWP, N_("SPI BIOS region"), },
    /* TRANSLATORS: Title: SPI refers to the flash chip in the computer */
    { FWUPD_SECURITY_ATTR_ID_SPI_DESCRIPTOR, N_("SPI BIOS Descriptor"), },
    /* TRANSLATORS: Title: DMA as in https://en.wikipedia.org/wiki/DMA_attack  */
    { FWUPD_SECURITY_ATTR_ID_ACPI_DMAR, N_("Pre-boot DMA protection is"), },
    /* TRANSLATORS: Title: BootGuard is a trademark from Intel */
    { FWUPD_SECURITY_ATTR_ID_INTEL_BOOTGUARD_ENABLED, N_("Intel BootGuard"), },
    /* TRANSLATORS: Title: BootGuard is a trademark from Intel,
     * verified boot refers to the way the boot process is verified */
    { FWUPD_SECURITY_ATTR_ID_INTEL_BOOTGUARD_VERIFIED, N_("Intel BootGuard verified boot"), },
    /* TRANSLATORS: Title: BootGuard is a trademark from Intel,
     * ACM means to verify the integrity of Initial Boot Block */
    { FWUPD_SECURITY_ATTR_ID_INTEL_BOOTGUARD_ACM, N_("Intel BootGuard ACM protected"), },
    /* TRANSLATORS: Title: BootGuard is a trademark from Intel,
     * error policy is what to do on failure */
    { FWUPD_SECURITY_ATTR_ID_INTEL_BOOTGUARD_POLICY, N_("Intel BootGuard error policy"), },
    /* TRANSLATORS: Title: BootGuard is a trademark from Intel,
     * OTP = one time programmable */
    { FWUPD_SECURITY_ATTR_ID_INTEL_BOOTGUARD_OTP, N_("Intel BootGuard OTP fuse"), },
    /* TRANSLATORS: Title: CET = Control-flow Enforcement Technology,
     * enabled means supported by the processor */
    { FWUPD_SECURITY_ATTR_ID_INTEL_CET_ENABLED, N_("Intel CET"), },
    /* TRANSLATORS: Title: CET = Control-flow Enforcement Technology,
     * active means being used by the OS */
    { FWUPD_SECURITY_ATTR_ID_INTEL_CET_ACTIVE, N_("Intel CET Active"), },
    /* TRANSLATORS: Title: SMAP = Supervisor Mode Access Prevention */
    { FWUPD_SECURITY_ATTR_ID_INTEL_SMAP, N_("Intel SMAP"), },
    /* TRANSLATORS: Title: Memory contents are encrypted, e.g. Intel TME */
    { FWUPD_SECURITY_ATTR_ID_ENCRYPTED_RAM, N_("Encrypted RAM"), },
    /* TRANSLATORS: Title:
     * https://en.wikipedia.org/wiki/Input%E2%80%93output_memory_management_unit */
    { FWUPD_SECURITY_ATTR_ID_IOMMU, N_("IOMMU device protection"), },
    /* TRANSLATORS: Title: lockdown is a security mode of the kernel */
    { FWUPD_SECURITY_ATTR_ID_KERNEL_LOCKDOWN, N_("Kernel lockdown"), },
    /* TRANSLATORS: Title: if it's tainted or not */
    { FWUPD_SECURITY_ATTR_ID_KERNEL_TAINTED, N_("Kernel tainted"), },
    /* TRANSLATORS: Title: swap space or swap partition */
    { FWUPD_SECURITY_ATTR_ID_KERNEL_SWAP, N_("Linux swap"), },
    /* TRANSLATORS: Title: sleep state */
    { FWUPD_SECURITY_ATTR_ID_SUSPEND_TO_RAM, N_("Suspend-to-ram"), },
    /* TRANSLATORS: Title: a better sleep state */
    { FWUPD_SECURITY_ATTR_ID_SUSPEND_TO_IDLE, N_("Suspend-to-idle"), },
    /* TRANSLATORS: Title: PK is the 'platform key' for the machine */
    { FWUPD_SECURITY_ATTR_ID_UEFI_PK, N_("UEFI platform key"), },
    /* TRANSLATORS: Title: SB is a way of locking down UEFI */
    { FWUPD_SECURITY_ATTR_ID_UEFI_SECUREBOOT, N_("Secure boot"), },
    /* TRANSLATORS: Title: PCRs (Platform Configuration Registers) shouldn't be empty */
    { FWUPD_SECURITY_ATTR_ID_TPM_EMPTY_PCR, N_("All TPM PCRs are"), },
    /* TRANSLATORS: Title: the PCR is rebuilt from the TPM event log */
    { FWUPD_SECURITY_ATTR_ID_TPM_RECONSTRUCTION_PCR0, N_("TPM PCR0 reconstruction"), },
    /* TRANSLATORS: Title: TPM = Trusted Platform Module */
    { FWUPD_SECURITY_ATTR_ID_TPM_VERSION_20, N_("TPM v2.0"), },
    /* TRANSLATORS: Title: MEI = Intel Management Engine */
    { FWUPD_SECURITY_ATTR_ID_MEI_MANUFACTURING_MODE, N_("MEI manufacturing mode"), },
    /* TRANSLATORS: Title: MEI = Intel Management Engine, and the
     * "override" is the physical PIN that can be driven to
     * logic high -- luckily it is probably not accessible to
     * end users on consumer boards */
    { FWUPD_SECURITY_ATTR_ID_MEI_OVERRIDE_STRAP, N_("MEI override"), },
    /* TRANSLATORS: Title: MEI = Intel Management Engine */
    { FWUPD_SECURITY_ATTR_ID_MEI_VERSION, N_("MEI version"), },
    /* TRANSLATORS: Title: if firmware updates are available */
    { FWUPD_SECURITY_ATTR_ID_FWUPD_UPDATES, N_("Firmware updates"), },
    /* TRANSLATORS: Title: if we can verify the firmware checksums */
    { FWUPD_SECURITY_ATTR_ID_FWUPD_ATTESTATION, N_("Firmware attestation"), },
    /* TRANSLATORS: Title: if the fwupd plugins are all present and correct */
    { FWUPD_SECURITY_ATTR_ID_FWUPD_PLUGINS, N_("fwupd plugins"), },
    /* TRANSLATORS: Title: Direct Connect Interface (DCI) allows
     * debugging of Intel processors using the USB3 port */
    { FWUPD_SECURITY_ATTR_ID_INTEL_DCI_ENABLED, N_("Intel DCI debugger"), },
    { FWUPD_SECURITY_ATTR_ID_INTEL_DCI_LOCKED, N_("Intel DCI debugger"), },
    /* TRANSLATORS: Title: DMA as in https://en.wikipedia.org/wiki/DMA_attack  */
    { FWUPD_SECURITY_ATTR_ID_PREBOOT_DMA_PROTECTION, N_("Pre-boot DMA protection"), },
    /* TRANSLATORS: Title: if fwupd supports HSI on this chip */
    { FWUPD_SECURITY_ATTR_ID_SUPPORTED_CPU, N_("Supported CPU"), }
  };

  for (int i = 0; i < G_N_ELEMENTS (attr_to_name); i++)
  {
    if (g_strcmp0 (appstream_id, attr_to_name[i].id) == 0)
      return _(attr_to_name[i].name);
  }

  return appstream_id;
}

gboolean
firmware_security_attr_has_flag (guint64                flags,
                                 FwupdSecurityAttrFlags flag)
{
  return (flags & flag) > 0;
}

void
load_custom_css (const char *path)
{
  g_autoptr (GtkCssProvider) provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, path);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
}
