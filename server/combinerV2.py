from pymongo import MongoClient
from pathlib import PureWindowsPath
import os
import re
import struct
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading
import time


client = MongoClient("mongodb://localhost:27017/")
db = client["user_data"]
MAX_THREADS = 5

base_dir = r"C:\Users\casio\Desktop\uploader\6260_2897754391"

def combine_file_parts(folder_path, filename):

    part_files = sorted(
        [f for f in os.listdir(folder_path) if f.startswith(f"{filename}_part")],
        key=lambda x: int(x.split('_part')[-1])
    )

    if not part_files:
        print("[-] No part files found in the specified folder." , folder_path, filename)
        return False

    # Determine base filename from the first file name (e.g., "procmon.exe" from "procmon.exe_part1")
    first_file = part_files[0]
    base_filename = re.split(r"_part\d+$", first_file)[0]
    output_filename = f'\\{base_filename}_comp'
    a = f'{folder_path}'+ output_filename

    # Write each part to the combined file
    with open(a, "wb") as output_file:
        for part in part_files:
            part_path = folder_path + "\\"+ part
            with open(part_path, 'rb') as f:
                part_data = f.read()
                output_file.write(struct.pack('I', len(part_data)))  # I = 4-byte unsigned integer
                output_file.write(part_data)
                f.close()

            os.remove(part_path)

        output_file.close()
    
    return True


def convert_path(base_path: str, new_root: str) -> str:
    old = PureWindowsPath(new_root)
    base_path = PureWindowsPath(base_path)

    if new_root.startswith("\\\\"):

        relative = PureWindowsPath(*old.parts[1:])

    else:
        
        drive_root = PureWindowsPath(old.drive + "\\")
        relative = old.relative_to(drive_root)

    new_path = base_path / relative

    return "\\\\?\\" + str(new_path)



def check_file_parts(path, total_parts):
    # Keep the given path exactly as is (supports long paths)
    directory = os.path.dirname(path)
    filename = os.path.basename(path)

    for i in range(1, total_parts + 1):
        part_name = f"{filename}_part{i}"
        part_path = os.path.join(directory, part_name)

        if not os.path.exists(part_path):
            print(f"Missing: {part_path}")
            return False

    return True


def worker(item):
    try:
        file_path = convert_path(base_dir, item["PATH"])
        for file in item["files"]:
            final_file_path = file_path + "\\" + file["file_name"]
            if check_file_parts(final_file_path , file["total_part"]):
                combine_file_parts(file_path,file["file_name"])
    except Exception as e:
        print(e)

def combiner(collection_name):
    
    result = db[collection_name + "_upload"].find({})

    with ThreadPoolExecutor(max_workers=MAX_THREADS) as executor:
        futures = []

        for item in result:  
            futures.append(executor.submit(worker, item))
        
        for future in as_completed(futures):
            future.result()



if __name__ == "__main__":

    combiner("2897754391_6260")