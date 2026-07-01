from pymongo import MongoClient
import sys
import socket
import threading
from pathlib import Path
import os
import struct
import re
import time
import json
import traceback
from datetime import datetime
import random
from colorama import init, Fore, Style

init()

host = '0.0.0.0'
port = 4501

client_sockets = []

new_base_folder=sys.argv[1]
if not os.path.exists(new_base_folder):
    os.makedirs(new_base_folder)

client = MongoClient("mongodb://localhost:27017/")
db = client["user_data"]
collection = db["datas"]
lock = threading.Lock()

def remove_task(client_id,task_id):
    result = db["task_manager"].update_one(
        # {"client_id": client_id}, {"$pull": {"tasks" : {"task_id": task_id}}})
            {"client_id": client_id, "tasks.task_id": task_id},
            {"$set": {"tasks.$.status": "finished"}}
            )
    if result.matched_count == 0:
        log(f"task {task_id} for {client_id} didn't remove!",3)
        return False
    else:
        log(f"task {task_id} for {client_id} removed!",1)
        return True
    
def escape_regex_special_chars_except_backslash(text):
    """
    Escapes all regex special characters except for the backslash (\).
    """
    # List of special regex characters excluding backslash
    special_chars = ".^$*+?{}[]|()"
    
    escaped = ''
    for char in text:
        if char in special_chars:
            escaped += '\\' + char
        else:
            escaped += char
    return escaped


def update_user_task_status(client_id, tasks):
    

    for task in tasks:

        result = db["task_manager"].update_one(
            {"client_id": client_id, "tasks.task_id": task["task_id"]},
            {"$set": {"tasks.$.status": "in_progress"}}
            )

def add_compelete_load_to_database(client_id, task_id, complete_loaded_data):

    # 1- first check if this folder and sub folders exist remove them
    try:
        complete_loaded_data_str = complete_loaded_data.decode('utf-8').replace('\\', '\\\\')
        # print(complete_loaded_data_str[0:30])
        json_complete_loaded_data = json.loads(complete_loaded_data_str)

        for item in json_complete_loaded_data:

            # item["PATH"] = item["PATH"].replace('.', '\\.')
            # item["PATH"] = item["PATH"].replace('$', '\\$')
            item["PATH"] = escape_regex_special_chars_except_backslash(item["PATH"])
            regex = re.compile(rf'^{item["PATH"]}.*', re.IGNORECASE)
            result = db[str(client_id) + "_load"].delete_many({ "PATH": { "$regex": regex } })

            # if not result.deleted_count > 0:
            #     log(f"No matching documents found. for {item['PATH']}", 3)

        result = import_to_mongodb(client_id, complete_loaded_data)

        if not result:
            return False
        
        return True

    except Exception as e:
        log(f"Error in add_compelete_load_to_database: {e}", 4)
        return False

def get_clientid_from_socket(client_socket):

    clientid = None
    for item in client_sockets:
        if item["socket"] == client_socket:
            clientid = item["client"]
    
    if not clientid:
        log(f"client not found for socket requested!", 3)

    return clientid


def get_client_socket_handle(client_id):
    client_socket = None
    for item in client_sockets:
        if item["client"] == client_id:
            client_socket = item["socket"]
    
    if not client_socket:
        log(f"client {client_id} socket requested but didn't found!", 3)

    return client_socket

def log(content,type = 0):
    current_time = "[" + datetime.now().strftime("%H:%M:%S") + "]"
    if type == 0:
        # normal information
        print(Fore.CYAN + "[i]", content, current_time + Style.RESET_ALL)

    elif type == 1:
        # good information
        print(Fore.GREEN + "[+]", content, current_time + Style.RESET_ALL)
        
    elif type == 2:
        # warning
        print(Fore.YELLOW + "[!]" , content, current_time + Style.RESET_ALL)

    elif type == 3:
        # error
        print("[-]", content, current_time)

    elif type == 4:
        # critical error
        print(Fore.RED + "[*]", content, current_time + Style.RESET_ALL)

def send_tasks_to_client(task, client_id):

    client_socket = get_client_socket_handle(client_id)

    if not client_socket:

        log(f"{client_id} socket didn't found!", 3)
        return
            
    try:

        tasks_as_json = json.dumps(task)
        task_len = len(tasks_as_json)
        task_len_bytes = task_len.to_bytes(4, byteorder='little', signed=True)
        client_socket.send(task_len_bytes)
        client_socket.send(tasks_as_json.encode('utf-8'))

        update_user_task_status(client_id, task)

    except (ConnectionResetError, ConnectionAbortedError, BrokenPipeError, TimeoutError, socket.timeout) as e:
        
        log(f"socket send_tasks_to_client: {e}", 4)
        result = turn_client_offline(client_id)
        client_socket.close()
        
    except Exception as e:
        log(f"send_tasks_to_client: {e}", 4)



def check_user_tasks(client_id, initial = False):

    log(f"checking user tasks {client_id}")
    # result = db["task_manager"].find_one({"client_id" : client_id, "tasks.status" : "pending"} , {"tasks": {"$elemMatch": {"status": "pending"}}, "_id" : 0})

    result = db["task_manager"].find_one({"client_id" : client_id})

    task_list = []
    for item in result["tasks"]:
        if item["status"] == "pending":
            task_list.append(item)

        elif item["status"] == "in_progress" and item["task_flag"] == 4 and initial:  

            item["is_resume"] = True
            item["pathes"] = dump_mongodb_files(client_id, item["task_id"])
            task_list.append(item)

        else:
            continue

    if not result:
        log(f" any task not found for {client_id}")
        return

    # print(task_list)

    if len(result["tasks"]) > 0:
        log("user tasks sending to client...")
        # send_tasks_to_client(result["tasks"], client_id)
        send_tasks_to_client(task_list, client_id)
        # db["task_manager"].find_one({"client_id" : client_id},{"tasks.$.status" : 0})
    else:
        log(f"we didn't find any task for {client_id}")


def check_part_exist(task_id, original_path, part, client_id):

    folder_path = os.path.dirname(original_path)
    if folder_path[-1] == "\\" or folder_path[-1] == "/":
        folder_path = folder_path[:-1]
    file_name = os.path.basename(original_path)

    result = db[str(client_id) + "_" + str(task_id) + "_" + "upload"].find_one({"PATH" : folder_path, "files.file_name" : file_name, "files.parts" : part})

    if result:
        return True
    else:
        return False

def submit_client(client_id):

    result1 = db["clients_status"].insert_one({"client_name" : str(client_id) , "client_id" : client_id, "client_status" : True })
    result2 = db["task_manager"].insert_one({"client_id" : client_id, "tasks" : [{"task_flag" : 1, "status" : "pending" ,"task_id" : random.randint(1000, 9999) }]})

    if result1.inserted_id and result2.inserted_id:
        return True
    else:
        return False

def turn_client_offline(client_id):

    if not client_id:
        log(f"{client_id} didn't found!", 3)

    result = db["clients_status"].update_one({"client_id" : client_id}, {"$set" : { "client_status" : False}})
    
    if result.matched_count == 0:
        return False
    else:
        log(f"client {client_id} truned off!", 3)
        return True

def turn_client_online(client_id):
    result = db["clients_status"].update_one({"client_id" : client_id}, {"$set" : { "client_status" : True}})
    
    if result.matched_count == 0:
        return False
    else:
        return True

def welcom_user(client_id,client_socket):

    # Default values should get from controler server
    chunk_size = 1 # 1MB
    threads = 15 # Threads for read files
    volume = 0
    sleep_time = 0

    send_all_flag_bytes = b''
    send_all_flag_bytes += chunk_size.to_bytes(4, byteorder='little', signed=True)
    send_all_flag_bytes += threads.to_bytes(4, byteorder='little', signed=True)
    send_all_flag_bytes += volume.to_bytes(4, byteorder='little', signed=True)
    send_all_flag_bytes += sleep_time.to_bytes(4, byteorder='little', signed=True)


    try:
        client_socket.send(send_all_flag_bytes)

        log("welcom package has been sent!")

    except Exception as e:
        log(f"Exception type: {type(e).__name__}",4)
        log(f"Cause: {e}",4)
        traceback.print_exc()
        #socket_error_handling(client_id,client_socket)

def wait_until_idle(database="admin", poll_interval=1):

    db = client[database]
    while True:
        current_ops = db.command("currentOp", {"active": True})
        ops = current_ops.get("inprog", [])

        active_user_ops = []
        for op in ops:
            # Skip if this is our own connection
            if op.get("client") and "127.0.0.1" in op["client"]:
                continue

            # Skip idle/waiting operations
            if op.get("waitingForLatch") or op.get("waitingForLock"):
                continue

            # Focus only on user-type ops
            if op.get("op") in ("query", "command") and not op.get("system"):
                active_user_ops.append(op)

        if not active_user_ops:
            print("[*] MongoDB is idle.")
            break

        print(f"[*] {len(active_user_ops)} user operation(s) still running...")
        time.sleep(poll_interval)

def dump_mongodb_files(client_id, task_id):

    result = db[str(client_id) + "_" + str(task_id) + "_" + "upload"].find({},{"_id" : 0, "files.is_combined?" : 0})
    result = list(result)
    for value in result[:]:  # shallow copy to safely modify 'result'
        files = value["files"]
        for i in range(len(files) - 1, -1, -1):  # reverse loop
            item = files[i]
            if isinstance(item, dict):
                existing_parts = set(item["parts"])
                full_set = set(range(1, int(item["total_part"]) + 1))
                not_exist_parts = sorted(list(full_set - existing_parts))
                if not not_exist_parts:
                    del files[i]  # safe to delete by index
                else:
                    item["parts"] = not_exist_parts
        if not files:
            result.remove(value)
            
    json_bytes = str(json.dumps(result, separators=(",", ":"), ensure_ascii=False))
    # json_bytes = json.dumps(result).encode("utf-8")
    # return {"data" : json_bytes , "len" : len(json_bytes)}
    return json_bytes

def remove_from_database(original_path, client_id, total_part, part, task_id):

    try:
        folder_path = os.path.dirname(original_path)
        if folder_path[-1] == "\\" or folder_path[-1] == "/":
            folder_path = folder_path[:-1]

        file_name = os.path.basename(original_path)

        with lock:
            # if int(total_part) == -1:
            #     log(f"client coudn't open this file: {folder_path}/{file_name} part:{part}", 2)
            #     result = db[str(client_id)].update_one({"PATH": folder_path, "files.file_name" : file_name}, {"$pull": {"files": file_name}})
            #     if result.matched_count == 0:
            #         print("[-] File not found: ", "remove files", folder_path, file_name, total_part, part)

            if (file_result := db[str(client_id) + "_" + str(task_id) + "_upload"].find_one({"PATH": folder_path, "files.file_name" : file_name}, {"files.$" : 1, "_id" : 0})):
                
                result = db[str(client_id) + "_" + str(task_id) + "_upload"].update_one({"PATH": folder_path, "files.file_name" : file_name}, {"$push": {"files.$.parts": int(part)}})
                if result.matched_count == 0:
                    print("[-] Error in updating: ", "push file parts", file_name, total_part, part)
                    return
                
                # file_result = file_result["files"][0]
                # file_result["parts"].append(int(part))
                # if (file_result["total_part"] <= (len(file_result["parts"]))) and not file_result["is_combined?"]:
                #     existing_parts = set(file_result["parts"])
                #     full_set = set(range(1, int(file_result["total_part"]) + 1))
                #     not_exist_parts = sorted(list(full_set - existing_parts))

                #     if not not_exist_parts:
                #         if combine_file_parts(original_path, task_id, client_id):
                #             result = db[str(client_id) + "_" + str(task_id) + "_upload"].update_one({"PATH": folder_path, "files.file_name" : file_name}, {"$set": {"files.$.is_combined?": True}})
                #             if result.matched_count == 0:
                #                 print("[-] Error in updating: ", "push file parts", file_name, total_part, part)

            else:
                # print("[-] file not found! file:", original_path , folder_path, file_name)
                result = db[str(client_id) + "_" + str(task_id) + "_upload"].insert_one(
                    {
                        "PATH" : folder_path,
                        "files" : [
                            {
                                "file_name" : file_name,
                                "is_combined?" : True if int(total_part) == 1 else False,
                                "total_part" : total_part,
                                "parts" : [part]
                            }
                        ]
                    }
                )

                if not result.inserted_id:
                    log(f"we coudn't insert data for {client_id} and task {task_id}",4)
                    return
                
                if int(total_part) == 1:
                    combine_file_parts(original_path, task_id, client_id)


    except Exception as e:
        print("[-] remove_from_database: " + str(e))

def split_into_chunks(lst, chunk_size):
    return [lst[i:i + chunk_size] for i in range(0, len(lst), chunk_size)]

def import_to_mongodb(client_id, json_data, suffix = "_load"):

    try:
        
        # print(json_data)
        json_str = json_data.decode("utf-8")
        json_data = json.loads(json_str)

       
        for item in json_data:
            if len(item["files"]) > 200:
                chunks = split_into_chunks(item["files"], 200)
                for chunk in chunks:
                    dic = {
                        "PATH" : item["PATH"],
                        "files" : chunk
                    }
                    db[str(client_id) + suffix].insert_one(dic)
            else:
                db[str(client_id) + suffix ].insert_one(item)
        
        return True

    except Exception as e:
        log(f"Error in import_to_mongo: {e}", 4)

        return False

def check_id_in_mongodb(client_id):

    # collection_name = str(client_id)
    # if collection_name in db.list_collection_names():
    if db["clients_status"].find_one({"client_id" : client_id}):
        return True
    else:
        return False

def combine_file_parts(path, task_id, client_id):
    
    # path_without_drive = Path(str(path)).parts[1:]
    # file = Path(new_base_folder, *path_without_drive)

    path_parts = Path(path).parts[1:]
    new_base_drive = Path(new_base_folder)
    middle_folder = str(task_id) + "_" + str(client_id)

    new_path = new_base_drive / middle_folder / Path(*path_parts)

    filename = os.path.basename(path)
    folder_path = os.path.dirname(new_path)


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
   

def create_folders_and_file_in_myfolder(original_path, part, data, client_id, total_part, task_id):

    # path_without_drive = Path(original_path).parts[1:]
    # new_path = Path(new_base_folder, *path_without_drive)

    path_parts = Path(original_path).parts[1:]
    new_base_drive = Path(new_base_folder)
    middle_folder = str(task_id) + "_" + str(client_id)

    new_path = new_base_drive / middle_folder / Path(*path_parts)
    
    # Ensure the new folder structure exists
    folder_path = new_path.parent  
    if not folder_path.exists():
        folder_path.mkdir(parents=True, exist_ok=True)  
    
    # Create the file if it doesn't exist
    file_path = new_path
    if not file_path.exists():
        try:
            file = open(str(file_path) + "_part" + str(part) ,"wb")
            file.write(data)
            file.close()
        except Exception as e:
            print("create_folders_and_file_in_myfolder", e, "[error:]", file_path)
            print(repr(file_path))
            print(original_path)

        

        thread1 = threading.Thread(target=remove_from_database, args=(original_path,client_id, total_part, part, task_id))
        thread1.start()

        return True

    else:
        print("[-] File already exists under myfolder!")
        return False


def recive_data(client_socket, client_id, task_id):

    while True:
        # Receive the metadata (total_part + part + size_path + size_chunk + path + data)
        metadata = client_socket.recv(16)

        while len(metadata) < 16:
            metadata += client_socket.recv(16 - len(metadata))
            if len(metadata) == 0:
                log("thread exit1")
                return 0

        if not metadata:
            break

        # Extract the flag (first 4 bytes)
        total_part = int.from_bytes(metadata[0:4], byteorder='little', signed=True)
        part = int.from_bytes(metadata[4:8], byteorder='little')
        size_path = int.from_bytes(metadata[8:12], byteorder='little')
        size_chunk = int.from_bytes(metadata[12:16], byteorder='little')


        path = client_socket.recv(size_path)

        while len(path) < size_path:
            path += client_socket.recv(size_path - len(path))
            if len(path) == 0:
                print("thread exit2")
                return 0

        data = client_socket.recv(size_chunk)

        while len(data) < size_chunk:
            data += client_socket.recv(size_chunk - len(data))
            if len(data) == 0:
                print("thread exit3")
                return 0
        
        result = create_folders_and_file_in_myfolder(str(path.decode('utf-8')), part, data, client_id, total_part, task_id)

        if result:
            ok_answer = part.to_bytes(4, byteorder='little', signed=True)

            #time.sleep(random.uniform(3, 8))
            client_socket.send(ok_answer)
        else:
            print("[-] error in create_folders_and_file_in_myfolder result!")

def check_base_folder():
    if not os.path.exists(new_base_folder):
        print("base folder doesn't exist!")
        exit(0)

def f_recive(bytes_must_read,client_socket):

    try:
        bytesread = client_socket.recv(bytes_must_read)
        while len(bytesread) < bytes_must_read:
            bytesread += client_socket.recv(bytes_must_read - len(bytesread))
            if len(bytesread) == 0:
                log("socket closed clean")
                break
        return bytesread
    except Exception as e:
        clientid = get_clientid_from_socket(client_socket)
        turn_client_offline(clientid)
        log(f" f_recive {e}",4)

def setup_server():

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((host, port))
    server_socket.listen(100)

    while True:

        client_socket, addr = server_socket.accept()

        log("New socket!")
        
        bytesread = f_recive(4, client_socket)

        if not bytesread:
            log("bytesread was empty", 4)
            continue

        flag = int.from_bytes(bytesread[0:4], byteorder='little')

        if flag != 1111: # check user flag
            client_socket.close()
            log("flag not accepted and socket droped!", 2)
            continue

        else:  # hand shaking

            bytes_must_read = 4 # flag(4)

            bytesread = f_recive(bytes_must_read, client_socket)

            if not bytesread:
                    log("bytesread was empty", 4)
                    continue

            flag = int.from_bytes(bytesread[0:4], byteorder='little')

            # initial flag
            if flag == 4966 :
                bytes_must_read = 4 # client_id(4)
                bytesread = f_recive(bytes_must_read, client_socket)

                if not bytesread:
                    log("bytesread was empty", 4)
                    continue

                client_id = int.from_bytes(bytesread[0:4], byteorder='little')
                log(f"initial client {client_id}")

                if check_id_in_mongodb(client_id):
                    # we checked client exist in database or not

                    # first find client_id in  client_sockets
                    # then add socket to it
                    log("client found in database!")

                    # change client status to online
                    client_status = turn_client_online(client_id)
                    if not client_status:
                        log("client is online but can't change status!", 3)
                    else:
                        log(f"client {client_id} turn online")


                    client_founded_in_list = False
                    # for index, item in enumerate(client_sockets):
                    for item in client_sockets:
                        if item["client"] == client_id:
                            item["socket"] = client_socket
                            # client_sockets[index]["socket"] = client_id
                            client_founded_in_list = True
                            log(f"socket changed for client id: {client_id}",2)
                            break

                    if not client_founded_in_list:
                        client_sockets.append({
                        "client" : client_id,
                        "socket" : client_socket
                        })
                        log(f"client socket for {client_id} was not found in list so it added",2)
                    
                    welcom_user(client_id,client_socket)
                    check_user_tasks(client_id, True)

                    continue

                else:

                    submit_client_result = submit_client(client_id)

                    if not submit_client_result:
                        log(f"client {client_id} is submited but can't save on database!", 3)
                    else:
                        log(f"client {client_id} is submited")


                    client_sockets.append({
                        "client" : client_id,
                        "socket" : client_socket
                    })

                    log(f"new user accpeted: {client_id}",1)

                    welcom_user(client_id,client_socket)
                    check_user_tasks(client_id)

                    continue

            elif flag == 1113:

                log("client want check a part exist on server or not. (error 12)", 2)
                    
                bytes_must_read = 16 # (clientid + taskid + part + size_path + path)
                metadata = f_recive(bytes_must_read, client_socket)

                if not metadata:
                    log("metadat was empty", 4)
                    continue

                client_id = int.from_bytes(metadata[0:4], byteorder='little')
                task_id = int.from_bytes(metadata[4:8], byteorder='little')
                part = int.from_bytes(metadata[8:12], byteorder='little')
                size_path = int.from_bytes(metadata[12:16], byteorder='little')

                bytes_must_read = size_path
                path = f_recive(bytes_must_read, client_socket)

                if not path:
                    log("path was empty", 4)
                    continue


                client_socket = get_client_socket_handle(client_id)
                result = check_part_exist(task_id, path, part, client_id)
                if result:
                    # client_socket = get_client_socket_handle(client_id)
                    
                    task = { "tasks" : [ {"task_flag" : 5, "task_id" : random.randint(1000, 9999), "error12" : {"path" : path, "part" : part}}] }
                    send_tasks_to_client(task,client_id)
                    # result = db["task_manager"].update_one({"client_id" : client_id},{"$push" : { "tasks" : {"task_flag" : 5, "task_id" : random.randint(1000, 9999), "status" : "pending", "error12" : {"path" : path, "part" : part}}}})
                    # we should sent to client that part exist and remove it from vector or queue
                    None
                # else:
                #     # client_socket = get_client_socket_handle(client_id)

                #     task = { "tasks" : [{"task_flag" : 6, "task_id" : random.randint(1000, 9999), "error12" : {"path" : path, "part" : part}} ] }
                #     send_tasks_to_client(task,client_id)
                #     # result = db["task_manager"].update_one({"client_id" : client_id},{"$push" : { "tasks" : {"task_flag" : 6, "task_id" : random.randint(1000, 9999), "status" : "pending", "error12" : {"path" : path, "part" : part}}}})
                #     # part not exist and client should send it again!
                #     None

            elif flag == 7921:
                
                log(f"load complete recived! from {client_id}")

                bytes_must_read = 12 # (client_id + data_size)
                metadata = f_recive(bytes_must_read, client_socket)

                if not metadata:
                    log("metadata was empty", 4)
                    continue

                client_id = int.from_bytes(metadata[0:4], byteorder='little')
                task_id = int.from_bytes(metadata[4:8], byteorder='little')
                # error_id = int.from_bytes(metadata[4:8], byteorder='little')
                data_size = int.from_bytes(metadata[8:12], byteorder='little')

                bytes_must_read = data_size # (data)
                complete_loaded_data = f_recive(bytes_must_read, client_socket)

                if not complete_loaded_data:
                    log("complete_loaded_data was empty", 4)
                    continue

                result = add_compelete_load_to_database(client_id, task_id, complete_loaded_data)
                if result:
                    log(f"Data loaded and task {task_id} for {client_id} will remove")
                    result = remove_task(client_id,task_id)
                    if result:
                        log(f"cilent {client_id} task {task_id} removed.")
                    else:
                        log(f"cilent {client_id} task {task_id} didn't removed!", 4)
                else:
                    log(f"Data didn't loaded task {task_id} for {client_id} will remove", 3)

            elif flag == 4456:
                # trap
                bytes_must_read = 4 # (client_id + data_size)
                metadata = f_recive(bytes_must_read, client_socket)

                if not metadata:
                    log("metadata was empty", 4)
                    continue

                client_id = int.from_bytes(metadata[0:4], byteorder='little')
                log(f"trap from {client_id}")
                check_user_tasks(client_id)
        
            elif flag == 3247:
                # upload file

                bytes_must_read = 8 # (client_id + taskid)
                metadata = f_recive(bytes_must_read, client_socket)

                if not metadata:
                    log("metadata was empty", 4)
                    continue

                client_id = int.from_bytes(metadata[0:4], byteorder='little')
                task_id = int.from_bytes(metadata[4:8], byteorder='little')
                thread1 = threading.Thread(target=recive_data, args=(client_socket, client_id, task_id))
                thread1.start()

            elif flag == 3248:
                
                #upload schema
                bytes_must_read = 12 # (client_id + taskid + data_size)
                metadata = f_recive(bytes_must_read, client_socket)

                if not metadata:
                    log("metadata was empty", 4)
                    continue

                client_id = int.from_bytes(metadata[0:4], byteorder='little')
                task_id = int.from_bytes(metadata[4:8], byteorder='little')
                data_size = int.from_bytes(metadata[8:12], byteorder='little')

                bytes_must_read = data_size
                complete_loaded_data = f_recive(bytes_must_read, client_socket)

                if not metadata:
                    log("metadata was empty", 4)
                    continue

                # print(complete_loaded_data)
                import_to_mongodb(client_id, complete_loaded_data, "_" + str(task_id) + "_" + "upload")

            elif flag == 4935:

                # end upload flag
                log(f"{client_id} want task removing!")
                bytes_must_read = 8
                metadata = f_recive(bytes_must_read, client_socket)

                if not metadata:
                    log("metadata was empty", 4)
                    continue

                client_id = int.from_bytes(metadata[0:4], byteorder='little')
                task_id = int.from_bytes(metadata[4:8], byteorder='little')

                result = remove_task(client_id,task_id)

                if result:
                    log(f"cilent {client_id} task {task_id} removed.")
                else:
                    log(f"cilent {client_id} task {task_id} didn't removed!", 4)


if __name__ == "__main__":
    check_base_folder()
    setup_server()
