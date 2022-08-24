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

/* we don't need to keep this up to date, as any new attrs added by fwupd >= 1.8.3 will also
 * come with translated titles *and* descriptions */
static const gchar *
fu_security_attr_get_title_fallback (const gchar *appstream_id)
{
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_SPI_BIOSWE) == 0)
    {
      /* TRANSLATORS: Title: firmware refers to the flash chip in the computer */
      return _("Firmware Write Protection");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_SPI_BLE) == 0)
    {
      /* TRANSLATORS: Title: firmware refers to the flash chip in the computer */
      return _("Firmware Write Protection Lock");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_SPI_SMM_BWP) == 0)
    {
      /* TRANSLATORS: Title: SPI refers to the flash chip in the computer */
      return _("Firmware BIOS Region");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_SPI_DESCRIPTOR) == 0)
    {
      /* TRANSLATORS: Title: firmware refers to the flash chip in the computer */
      return _("Firmware BIOS Descriptor");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_PREBOOT_DMA_PROTECTION) == 0)
    {
      /* TRANSLATORS: Title: DMA as in https://en.wikipedia.org/wiki/DMA_attack  */
      return _("Pre-boot DMA Protection");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_INTEL_BOOTGUARD_ENABLED) == 0)
    {
      /* TRANSLATORS: Title: BootGuard is a trademark from Intel */
      return _("Intel BootGuard");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_INTEL_BOOTGUARD_VERIFIED) == 0)
    {
      /* TRANSLATORS: Title: BootGuard is a trademark from Intel,
       * verified boot refers to the way the boot process is verified */
      return _("Intel BootGuard Verified Boot");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_INTEL_BOOTGUARD_ACM) == 0)
    {
      /* TRANSLATORS: Title: BootGuard is a trademark from Intel,
       * ACM means to verify the integrity of Initial Boot Block */
      return _("Intel BootGuard ACM Protected");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_INTEL_BOOTGUARD_POLICY) == 0)
    {
      /* TRANSLATORS: Title: BootGuard is a trademark from Intel,
       * error policy is what to do on failure */
      return _("Intel BootGuard Error Policy");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_INTEL_BOOTGUARD_OTP) == 0)
    {
      /* TRANSLATORS: Title: BootGuard is a trademark from Intel */
      return _("Intel BootGuard Fuse");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_INTEL_CET_ENABLED) == 0)
    {
      /* TRANSLATORS: Title: CET = Control-flow Enforcement Technology,
       * enabled means supported by the processor */
      return _("Intel CET Enabled");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_INTEL_CET_ACTIVE) == 0)
    {
      /* TRANSLATORS: Title: CET = Control-flow Enforcement Technology,
       * active means being used by the OS */
      return _("Intel CET Active");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_INTEL_SMAP) == 0)
    {
      /* TRANSLATORS: Title: SMAP = Supervisor Mode Access Prevention */
      return _("Intel SMAP");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_ENCRYPTED_RAM) == 0)
    {
      /* TRANSLATORS: Title: Memory contents are encrypted, e.g. Intel TME */
      return _("Encrypted RAM");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_IOMMU) == 0)
    {
      /* TRANSLATORS: Title:
       * https://en.wikipedia.org/wiki/Input%E2%80%93output_memory_management_unit */
      return _("IOMMU Protection");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_KERNEL_LOCKDOWN) == 0)
    {
      /* TRANSLATORS: Title: lockdown is a security mode of the kernel */
      return _("Linux Kernel Lockdown");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_KERNEL_TAINTED) == 0)
    {
      /* TRANSLATORS: Title: if it's tainted or not */
      return _("Linux Kernel Verification");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_KERNEL_SWAP) == 0)
    {
      /* TRANSLATORS: Title: swap space or swap partition */
      return _("Linux Swap");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_SUSPEND_TO_RAM) == 0)
    {
      /* TRANSLATORS: Title: sleep state */
      return _("Suspend To RAM");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_SUSPEND_TO_IDLE) == 0)
    {
      /* TRANSLATORS: Title: a better sleep state */
      return _("Suspend To Idle");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_UEFI_PK) == 0)
    {
      /* TRANSLATORS: Title: PK is the 'platform key' for the machine */
      return _("UEFI Platform Key");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_UEFI_SECUREBOOT) == 0)
    {
      /* TRANSLATORS: Title: SB is a way of locking down UEFI */
      return _("UEFI Secure Boot");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_TPM_EMPTY_PCR) == 0)
    {
      /* TRANSLATORS: Title: PCRs (Platform Configuration Registers) shouldn't be empty */
      return _("TPM Platform Configuration");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_TPM_RECONSTRUCTION_PCR0) == 0)
    {
      /* TRANSLATORS: Title: the PCR is rebuilt from the TPM event log */
      return _("TPM Reconstruction");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_TPM_VERSION_20) == 0)
    {
      /* TRANSLATORS: Title: TPM = Trusted Platform Module */
      return _("TPM v2.0");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_MEI_MANUFACTURING_MODE) == 0)
    {
      /* TRANSLATORS: Title: MEI = Intel Management Engine */
      return _("Intel Management Engine Manufacturing Mode");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_MEI_OVERRIDE_STRAP) == 0)
    {
      /* TRANSLATORS: Title: MEI = Intel Management Engine, and the "override" is enabled
       * with a jumper -- luckily it is probably not accessible to end users on consumer
       * boards */
      return _("Intel Management Engine Override");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_MEI_VERSION) == 0)
    {
      /* TRANSLATORS: Title: MEI = Intel Management Engine */
      return _("Intel Management Engine Version");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_FWUPD_UPDATES) == 0)
    {
      /* TRANSLATORS: Title: if firmware updates are available */
      return _("Firmware Updates");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_FWUPD_ATTESTATION) == 0)
    {
      /* TRANSLATORS: Title: if we can verify the firmware checksums */
      return _("Firmware Attestation");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_FWUPD_PLUGINS) == 0)
    {
      /* TRANSLATORS: Title: if the fwupd plugins are all present and correct */
      return _("Firmware Updater Verification");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_PLATFORM_DEBUG_ENABLED) == 0 ||
      g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_PLATFORM_DEBUG_LOCKED) == 0)
    {
      /* TRANSLATORS: Title: Allows debugging of parts using proprietary hardware */
      return _("Platform Debugging");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_SUPPORTED_CPU) == 0)
    {
      /* TRANSLATORS: Title: if fwupd supports HSI on this chip */
      return _("Processor Security Checks");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_AMD_ROLLBACK_PROTECTION) == 0)
    {
      /* TRANSLATORS: Title: if firmware enforces rollback protection */
      return _("AMD Rollback Protection");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_AMD_SPI_REPLAY_PROTECTION) == 0)
    {
      /* TRANSLATORS: Title: if hardware enforces control of SPI replays */
      return _("AMD Firmware Replay Protection");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_AMD_SPI_WRITE_PROTECTION) == 0)
    {
      /* TRANSLATORS: Title: if hardware enforces control of SPI writes */
      return _("AMD Firmware Write Protection");
    }
  if (g_strcmp0 (appstream_id, FWUPD_SECURITY_ATTR_ID_PLATFORM_FUSED) == 0)
    {
      /* TRANSLATORS: Title: if the part has been fused */
      return _("Fused Platform");
    }
  return NULL;
}

const gchar *
fwupd_security_attr_result_to_string (FwupdSecurityAttrResult result)
{
  if (result == FWUPD_SECURITY_ATTR_RESULT_VALID)
    {
      /* TRANSLATORS: if the stauts is valid. For example security check is valid and key is valid. */
      return _("Valid");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_NOT_VALID)
    {
      /* TRANSLATORS: if the status or key is not valid. */
      return _("Not Valid");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_ENABLED)
    {
      /* TRANSLATORS: if the function is enabled through BIOS or OS settings. */
      return _("Enabled");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_NOT_ENABLED)
    {
      /* TRANSLATORS: if the function is not enabled through BIOS or OS settings. */
      return _("Not Enabled");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_LOCKED)
    {
      /* TRANSLATORS: the memory space or system mode is locked to prevent from malicious modification. */
      return _("Locked");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_NOT_LOCKED)
    {
      /* TRANSLATORS: the memory space or system mode is not locked. */
      return _("Not Locked");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_ENCRYPTED)
    {
      /* TRANSLATORS: The data is encrypted to prevent from malicious reading.  */
      return _("Encrypted");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_NOT_ENCRYPTED)
    {
      /* TRANSLATORS: the data in memory is plane text. */
      return _("Not Encrypted");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_TAINTED)
    {
      /* TRANSLATORS: Linux kernel is tainted by third party kernel module. */
      return _("Tainted");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_NOT_TAINTED)
    {
      /* TRANSLATORS: All the loaded kernel module are licensed. */
      return _("Not Tainted");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_FOUND)
    {
      /* TRANSLATORS: the feature can be detected. */
      return _("Found");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_NOT_FOUND)
    {
      /* TRANSLATORS: the feature can't be detected. */
      return _("Not Found");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_SUPPORTED)
    {
      /* TRANSLATORS: the function is supported by hardware. */
      return _("Supported");
    }
  if (result == FWUPD_SECURITY_ATTR_RESULT_NOT_SUPPORTED)
    {
      /* TRANSLATORS: the function isn't supported by hardware. */
      return _("Not Supported");
    }
  return NULL;
}


/* ->summary and ->description are translated */
FwupdSecurityAttr *
fu_security_attr_new_from_variant (GVariantIter *iter)
{
  FwupdSecurityAttr *attr = g_new0 (FwupdSecurityAttr, 1);
  const gchar *key;
  GVariant *value;

  while (g_variant_iter_next (iter, "{&sv}", &key, &value))
    {
      if (g_strcmp0 (key, "AppstreamId") == 0)
        attr->appstream_id = g_variant_dup_string (value, NULL);
      else if (g_strcmp0 (key, "Flags") == 0)
        attr->flags = g_variant_get_uint64(value);
      else if (g_strcmp0 (key, "HsiLevel") == 0)
        attr->hsi_level = g_variant_get_uint32 (value);
      else if (g_strcmp0 (key, "HsiResult") == 0)
        attr->result = g_variant_get_uint32 (value);
      else if (g_strcmp0 (key, "HsiResultFallback") == 0)
        attr->result_fallback = g_variant_get_uint32 (value);
      else if (g_strcmp0 (key, "Created") == 0)
        attr->timestamp = g_variant_get_uint64 (value);
      else if (g_strcmp0 (key, "Description") == 0)
        attr->description = g_strdup (dgettext ("fwupd", g_variant_get_string (value, NULL)));
      else if (g_strcmp0 (key, "Summary") == 0)
        attr->title = g_strdup (dgettext ("fwupd", g_variant_get_string (value, NULL)));
      g_variant_unref (value);
    }

  /* in fwupd <= 1.8.3 org.fwupd.hsi.Uefi.SecureBoot was incorrectly marked as HSI-0 */
  if (g_strcmp0 (attr->appstream_id, FWUPD_SECURITY_ATTR_ID_UEFI_SECUREBOOT) == 0)
    attr->hsi_level = 1;

  /* fallback for older fwupd versions */
  if (attr->appstream_id != NULL && attr->title == NULL)
    attr->title = g_strdup (fu_security_attr_get_title_fallback (attr->appstream_id));

  /* success */
  return attr;
}

void
fu_security_attr_free (FwupdSecurityAttr *attr)
{
  g_free (attr->appstream_id);
  g_free (attr->title);
  g_free (attr->description);
  g_free (attr);
}

gboolean
firmware_security_attr_has_flag (FwupdSecurityAttr     *attr,
                                 FwupdSecurityAttrFlags flag)
{
  return (attr->flags & flag) > 0;
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
