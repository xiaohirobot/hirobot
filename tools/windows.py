"""Windows build and guarded UART programming for this STM32F407 project."""
from pathlib import Path
from datetime import datetime
import argparse
import hashlib
import json
import logging
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'build/dual_link'
IMAGE = OUTPUT / 'hirobot_dual.bin'
MANIFEST = OUTPUT / 'build.json'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def build(args):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    # A failed build must not leave a previous image approved for flashing.
    MANIFEST.unlink(missing_ok=True)
    compiler = Path(args.compiler)
    gcc = compiler / 'arm-none-eabi-gcc.exe'
    for name in ['arm-none-eabi-gcc.exe', 'arm-none-eabi-objcopy.exe', 'arm-none-eabi-size.exe']:
        if not (compiler / name).is_file():
            raise RuntimeError(f'Missing compiler tool: {compiler / name}')
    peer = ROOT.parent / 'hirobot-ecw/firmware/robot_wire.h'
    if peer.exists() and peer.read_bytes() != (ROOT / 'src/components/robot_wire.h').read_bytes():
        raise RuntimeError('ECW and STM32 robot_wire.h differ; synchronize the protocol first')
    flags = ['-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=soft', '-ffreestanding',
             '-Wall', '-Wextra', '-Werror', '-O2', '-g', '-ffunction-sections',
             '-fdata-sections', '-I' + str(ROOT / 'src')]
    objects = []
    for source in sorted((ROOT / 'src').rglob('*.c')):
        obj = OUTPUT / 'obj' / source.relative_to(ROOT / 'src').with_suffix('.o')
        obj.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([str(gcc), *flags, '-c', str(source), '-o', str(obj)], check=True)
        objects.append(str(obj))
    elf = OUTPUT / 'hirobot_dual.elf'
    subprocess.run([str(gcc), *flags, '-nostdlib', '-T' + str(ROOT / 'STM32F407VETX_FLASH.ld'),
                    '-Wl,--gc-sections', '-Wl,-Map=' + str(OUTPUT / 'hirobot_dual.map'),
                    *objects, '-lgcc', '-lc', '-o', str(elf)], check=True)
    subprocess.run([str(compiler / 'arm-none-eabi-objcopy.exe'), '-O', 'binary', str(elf), str(IMAGE)], check=True)
    subprocess.run([str(compiler / 'arm-none-eabi-size.exe'), str(elf)], check=True)
    MANIFEST.write_text(json.dumps({'image': IMAGE.name, 'sha256': digest(IMAGE.read_bytes()),
                                   'address': 0x08000000, 'chip_id': 0x413}, indent=2), encoding='utf-8')
    print(f'Build OK: {IMAGE}', flush=True)


def validated_image():
    if not MANIFEST.exists():
        raise RuntimeError('No successful Windows build manifest. Run .\\build.ps1 first.')
    record = json.loads(MANIFEST.read_text(encoding='utf-8'))
    data = IMAGE.read_bytes()
    if (record.get('image') != IMAGE.name or record.get('sha256') != digest(data)
            or record.get('address') != 0x08000000 or record.get('chip_id') != 0x413):
        raise RuntimeError('Image/manifest mismatch. Run .\\build.ps1 again.')
    if not 8 <= len(data) <= 16384:
        raise RuntimeError('This flash helper supports sector 0 images only (8..16384 bytes).')
    sp = int.from_bytes(data[:4], 'little')
    reset = int.from_bytes(data[4:8], 'little')
    if not (0x20000000 < sp <= 0x20020000 and 0x08000001 <= reset < 0x08000000 + len(data) and reset & 1):
        raise RuntimeError('Invalid STM32 vector table; refusing to flash.')
    return data


def flash(args):
    if not re.fullmatch(r'COM[0-9]+', args.port):
        raise RuntimeError('Use a Windows COM port, for example COM14.')
    data = validated_image()
    if args.check_only:
        print(f'CHECK_ONLY_OK: {len(data)} bytes, SHA256 {digest(data)}; serial port not opened.')
        return
    dependencies = Path(args.loader_path)
    if dependencies.is_dir():
        sys.path.insert(0, str(dependencies))
    try:
        import serial
        from stm32loader.main import Stm32Loader
        from stm32loader.bootloader import Stm32Bootloader
    except ImportError as exc:
        raise RuntimeError('Missing stm32loader/pyserial. Set -LoaderPath to the prepared dependency directory.') from exc
    logging.basicConfig(level=logging.INFO, format='%(message)s', stream=sys.stdout)
    connection = serial.Serial(port=None, baudrate=115200, parity=serial.PARITY_EVEN, timeout=3)
    connection.dtr = False
    connection.rts = False
    connection.port = args.port
    loader = Stm32Loader()
    # Immutable input snapshot also prevents a concurrent build changing the bytes to write.
    backups = ROOT / 'build/backups'
    backups.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    snapshot = backups / f'{args.port}-{stamp}-write.bin'
    snapshot.write_bytes(data)
    loader.parse_arguments(['--port', args.port, '--family', 'F4', '--baud', '115200',
                            '--auto-erase', '--write', '--verify', '--no-progress', str(snapshot)])
    try:
        print(f'{args.port}: BOOT0 must be 1; press RESET before programming.', flush=True)
        connection.open()
        connection.reset_input_buffer()
        loader.stm32 = Stm32Bootloader(connection, device_family='F4')
        try:
            # Raw Serial has no enable_reset/enable_boot0 hooks; boot pins are manual.
            loader.stm32.reset_from_system_memory()
        except Exception as exc:
            raise RuntimeError('Bootloader handshake failed; no erase/write. Set BOOT0=1, press RESET, then retry.') from exc
        loader.detect_device()
        if loader.stm32.device.product_id != 0x413:
            raise RuntimeError('Unexpected chip ID; refusing to erase/write.')
        loader.read_flash_size()
        backup = bytes(loader.stm32.read_memory_data(0x08000000, 16384))
        if len(backup) != 16384:
            raise RuntimeError('Incomplete sector backup; refusing to erase/write.')
        backup_path = backups / f'{args.port}-{stamp}-sector0.bin'
        backup_path.write_bytes(backup)
        print(f'Sector 0 backup: {backup_path}', flush=True)
        loader.perform_commands()
        print('FLASH_AND_READBACK_OK. Set BOOT0=0 and press RESET to run.', flush=True)
    finally:
        if connection.is_open:
            connection.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    builder = commands.add_parser('build')
    builder.add_argument('--compiler', default='D:/MOCEworkplace/tools/st-windows/compiler/bin')
    programmer = commands.add_parser('flash')
    programmer.add_argument('--port', required=True)
    programmer.add_argument('--check-only', action='store_true')
    programmer.add_argument('--loader-path', default=str(ROOT.parent / 'hirobot-ecw/.cache/stm32loader-deps'))
    args = parser.parse_args()
    try:
        (build if args.command == 'build' else flash)(args)
    except (RuntimeError, OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f'ERROR: {exc}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
