import os
import subprocess
import requests
import zipfile
import logging
import time

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

def check_command(command):
    result = subprocess.run(['where', command], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return result.returncode == 0

def download_file(url, output_path):
    response = requests.get(url, stream=True)
    total_size = int(response.headers.get('content-length', 0))
    block_size = 1024
    downloaded_size = 0
    start_time = time.time()

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

def main():
    if not check_command('git'):
        logging.error("Git not found, please install Git.")
        return

    if not check_command('xmake'):
        logging.error("xmake not found, exiting...")
        return

    logging.info("Cloning the repository...")
    subprocess.run(['git', 'clone', '-b', 'header', 'git@github.com:LiteLDev/LeviLamina.git'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    os.chdir('LeviLamina')

    logging.info("Running xmake project...")
    subprocess.run(['xmake', 'project', '-k', 'compile_commands', 'build'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    downloads = [
        ("https://github.com/bdsmodding/Troll/releases/latest/download/Troll.exe", "Troll.exe"),
        ("https://github.com/LiteLDev/PreLoader/releases/latest/download/PreLoader.dll", "PreLoader.dll"),
        ("https://github.com/LiteLDev/bedrock-runtime-data/releases/latest/download/bedrock-runtime-data-windows-x64.zip", "bedrock-runtime-data-windows-x64.zip")
    ]

    for url, output_path in downloads:
        download_file(url, output_path)

    logging.info("Extracting bedrock-runtime-data-windows-x64.zip...")
    with zipfile.ZipFile('bedrock-runtime-data-windows-x64.zip', 'r') as zip_ref:
        zip_ref.extractall('.')
    os.remove('bedrock-runtime-data-windows-x64.zip')

    logging.info("Running Troll.exe...")
    subprocess.run(['Troll.exe', 'build', 'test/include_all.h'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

if __name__ == "__main__":
    main()
