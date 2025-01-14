import os
import stat
import subprocess
import requests
import zipfile
import logging
import time
import shutil
import argparse

work_dir = "./work"

logging.basicConfig(level=logging.INFO,
                    format='%(asctime)s - %(levelname)s - %(message)s')


def del_rw(action, name, exc):
    os.chmod(name, stat.S_IWRITE)
    os.remove(name)


def try_cd_to_work_dir():
    if os.path.exists(work_dir):
        shutil.rmtree(work_dir, onerror=del_rw)
    os.makedirs(work_dir, exist_ok=True)
    os.chdir(work_dir)


def check_command(command):
    result = subprocess.run(
        ['where', command], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return result.returncode == 0


def download_file(url, output_path):
    headers = {
        'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.3'}
    response = requests.get(url, headers=headers, stream=True)
    if response.status_code != 200:
        logging.error(
            f"Failed to download {url}. Status code: {response.status_code}")
        return False

    total_size = int(response.headers.get('content-length', 0))
    block_size = 1024
    downloaded_size = 0
    start_time = time.time()
    logging.info(f"Downloading {output_path}")
    with open(output_path, 'wb') as file:
        for data in response.iter_content(block_size):
            downloaded_size += len(data)
            file.write(data)
            done = int(50 * downloaded_size / total_size)
            elapsed_time = time.time() - start_time
            speed = downloaded_size / (1024 * elapsed_time)
            print(f"\r[{'=' * done}{' ' * (50-done)}] {downloaded_size / (1024 * 1024):.2f}/{total_size / (1024 * 1024):.2f} MB ({speed:.2f} KB/s)", end='\r')

    print()
    logging.info(f"Downloaded {output_path}")


def get_latest_server_release():
    url = "https://api.github.com/repos/LiteLDev/bedrock-runtime-data/releases"
    response = requests.get(url)
    releases = response.json()
    for release in releases:
        if release['tag_name'].endswith('-server'):
            return release['tag_name'], release['assets'][0]['browser_download_url']
    return None, None


def main():
    parser = argparse.ArgumentParser(description='Troll script')
    parser.add_argument('-e', '--ext-symbol-list', default=None,
                        help='External symbol list file path')
    parser.add_argument('-d', '--work-dir',
                        default='./work', help='Work directory')
    args = parser.parse_args()

    global work_dir
    work_dir = args.work_dir

    external_symbol_list = args.ext_symbol_list
    if external_symbol_list:
        if not os.path.exists(external_symbol_list):
            logging.error(
                f"External symbol list file not found: {external_symbol_list}")
            return
        external_symbol_list = os.path.abspath(external_symbol_list)

    if not check_command('git'):
        logging.error("Git not found, please install Git.")
        return

    if not check_command('xmake'):
        logging.error("xmake not found, exiting...")
        return

    try_cd_to_work_dir()

    logging.info("Cloning the repository...")
    subprocess.run(['git', 'clone', '-b', 'header',
                   'https://github.com/LiteLDev/LeviLamina.git'], shell=True)
    os.chdir('LeviLamina')

    logging.info("Running xmake project...")
    subprocess.run(['xmake', 'project', '-y', '-P', '.', '-k', 'compile_commands',
                   'build'], shell=True)

    downloads = [
        ("https://github.com/bdsmodding/Troll/releases/latest/download/Troll.exe", "Troll.exe"),
        ("https://github.com/LiteLDev/PreLoader/releases/latest/download/PreLoader.dll", "PreLoader.dll")
    ]

    tag_name, server_url = get_latest_server_release()
    if server_url:
        downloads.append((server_url, "bedrock-runtime-data-windows-x64.zip"))
        version = tag_name.rsplit('-server', 1)[0]
        logging.info(f"Latest server version: {version}")
    else:
        logging.error("No server version found.")
        return

    for url, output_path in downloads:
        download_file(url, output_path)

    logging.info("Extracting bedrock-runtime-data-windows-x64.zip...")
    with zipfile.ZipFile('bedrock-runtime-data-windows-x64.zip', 'r') as zip_ref:
        zip_ref.extractall('.')
    os.remove('bedrock-runtime-data-windows-x64.zip')

    logging.info("Downloading Bedrock Dedicated Server...")
    bedrock_server_url = f"https://www.minecraft.net/bedrockdedicatedserver/bin-win/bedrock-server-{version}.zip"
    download_file(bedrock_server_url, "bedrock-server.zip")

    logging.info("Extracting bedrock_server.exe...")
    with zipfile.ZipFile('bedrock-server.zip', 'r') as zip_ref:
        zip_ref.extract('bedrock_server.exe', '.')
    os.remove('bedrock-server.zip')

    logging.info("Writing include_all.h...")
    with open('test/include_all.h', 'w+') as f:
        for root, _, files in os.walk('src'):
            for file in files:
                if file.endswith('.h'):
                    file_path = os.path.relpath(os.path.join(root, file))
                    file_path = file_path.replace('\\', '/')
                    file_path = file_path.replace('src/', '')
                    f.write(f'#include "{file_path}"\n')
        pass

    logging.info("Running Troll.exe...")

    troll_args = ['Troll.exe', 'build', 'test/include_all.h',
                  'bedrock_server.exe', './', 'Troll.pdb']

    if external_symbol_list:
        troll_args.append(external_symbol_list)

    subprocess.run(troll_args, shell=True)

    logging.info(f'PDB generated at {os.path.abspath("Troll.pdb")}')


if __name__ == "__main__":
    main()
