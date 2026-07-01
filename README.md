# ByteRush

High-speed data transport framework for large-scale file transfers.

## Building

1. Install **zstdlib** using **vcpkg**.
2. Configure the server IP address in the client.
3. Build the project.

## Server Setup

1. Install **MongoDB**.
2. Install **Python**.
3. Install the required Python packages:

   ```bash
   pip install -r requirements.txt
   ```
4. Configure your firewall by either:

   * Opening the port used by the server, or
   * Temporarily disabling the firewall (not recommended outside of testing).
5. Start `ServerV3.py` and provide the destination directory as a command-line argument. This directory will be used to store the transferred files.
6. Edit `controller.py` and specify the target folder you want to transfer.
when new client connect to server it will assign new id to client and you can achive it from cmd console.
![Test Image 4](https://raw.githubusercontent.com/freedevco/ByteRush/refs/heads/main/img/Untitled.jpg)
8. Run the client. When the server console reports that a worker thread has been created, the transfer has started.

## Completing the Transfer

When the server console reports that the worker thread has finished, the transfer is complete. You can also monitor the transfer status in the MongoDB database.

To reconstruct the transferred data:

1. Run `combinerV2.py` to merge the file parts.
2. Run `decompress.py` to decompress the reconstructed files.


