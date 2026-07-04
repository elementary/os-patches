import os
import pytest
import subprocess

GRUB_SORT_VERSION = "debian/grub-sort-version"

def assert_grub_sort_version(input, output, flavour_order=""):
  env = os.environ.copy()
  env["LC_ALL"] = "C"
  env["GRUB_FLAVOUR_ORDER"] = flavour_order
  # Assert the output is as expected
  result = subprocess.run([GRUB_SORT_VERSION], env=env,
                          input=input, encoding="utf-8", capture_output=True)
  assert result.returncode == 0
  print(result.stdout)
  assert result.stdout == output
  # Then do the same in reverse mode too
  result = subprocess.run([GRUB_SORT_VERSION, "-r"], env=env,
                          input=input, encoding="utf-8", capture_output=True)
  assert result.returncode == 0
  assert result.stdout == "\n".join(output.rstrip().split("\n")[::-1]) + "\n"

def test_empty_line():
  INPUT = """/boot/vmlinuz-6.5.0-10-generic 2
/boot/vmlinuz-6.5.0-9-generic 2
 2
"""

  OUTPUT = """ 2
/boot/vmlinuz-6.5.0-9-generic 2
/boot/vmlinuz-6.5.0-10-generic 2
"""

  assert_grub_sort_version(INPUT, OUTPUT)

def test_custom_kernels():
  INPUT = """/boot/vmlinuz-6.6.0 2
/boot/vmlinuz-6.5.0-10-generic 2
/boot/vmlinuz-6.5.0-9-generic 2
/boot/vmlinuz-6.5.9 2
/boot/vmlinuz-6.5.8 2
"""

  OUTPUT = """/boot/vmlinuz-6.5.0-9-generic 2
/boot/vmlinuz-6.5.0-10-generic 2
/boot/vmlinuz-6.5.8 2
/boot/vmlinuz-6.5.9 2
/boot/vmlinuz-6.6.0 2
"""

  assert_grub_sort_version(INPUT, OUTPUT)

def test_flavour_order():
  INPUT = """/boot/vmlinuz-6.5.0-10-generic 2
/boot/vmlinuz-6.5.0-9-generic 2
/boot/vmlinuz-6.5.0-9-aaa 2
/boot/vmlinuz-6.5.0-9-aaa 1
/boot/vmlinuz-6.5.8 2
/boot/vmlinuz-6.5.9 2
/boot/vmlinuz-6.6.0 2
"""

  OUTPUT = """/boot/vmlinuz-6.5.8 2
/boot/vmlinuz-6.5.9 2
/boot/vmlinuz-6.6.0 2
/boot/vmlinuz-6.5.0-9-aaa 1
/boot/vmlinuz-6.5.0-9-aaa 2
/boot/vmlinuz-6.5.0-9-generic 2
/boot/vmlinuz-6.5.0-10-generic 2
"""

  assert_grub_sort_version(INPUT, OUTPUT, flavour_order="generic aaa")
