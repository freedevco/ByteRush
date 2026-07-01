import os
import zstandard as zstd
import struct


def decompress_zstd(data: bytes) -> bytes:

    dctx = zstd.ZstdDecompressor()
    return dctx.decompress(data)


def find_comp_files(root_dir):
    comp_files = []
    for dirpath, dirnames, filenames in os.walk(root_dir):
        for filename in filenames:
            if filename.endswith('_comp'):
                full_path = os.path.join(dirpath, filename)
                comp_files.append(full_path)
    return comp_files

def read_file_blocks(filepath):
    with open(filepath, 'rb') as f:
        write_in_file = open(filepath[:-5], 'wb')
        while True:

            length_bytes = f.read(4)
            if len(length_bytes) < 4:
                break  # EOF or corrupted
            
            block_size = struct.unpack('I', length_bytes)[0]
            data = f.read(block_size)

            # print(len(data), block_size)
            if len(data) < block_size:
                print("Warning: file ended unexpectedly.")
                break

            decompresed_data = decompress_zstd(data)
            write_in_file.write(decompresed_data)
        write_in_file.close()

    
    os.remove(filepath)

# Example usage
if __name__ == "__main__":
    root_directory = r"C:\Users\user1\Desktop\moscow\9378_4072159934"  # Change this to your target directory
    files = find_comp_files(root_directory)
    for file in files:
        print(file)
        read_file_blocks(file)


