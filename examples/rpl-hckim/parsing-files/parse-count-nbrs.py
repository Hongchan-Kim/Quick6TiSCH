import argparse

########## STEP-1: collect node information ##########

# STEP-1-1: declare variables and lists for node info (id and address)
ROOT_ID = 1
ROOT_ADDR = 'NULL'
non_root_id_list = list()
non_root_address_list = list()
all_id_list = list()

# STEP-1-2: extract node info (id and address)
parser = argparse.ArgumentParser()
parser.add_argument('any_scheduler')
parser.add_argument('any_iter')
parser.add_argument('any_id')
parser.add_argument('show_all')
args = parser.parse_args()

any_scheduler = args.any_scheduler
any_iter = args.any_iter
any_id = args.any_id
show_all = args.show_all

print('----- evaluation info -----')
print('any_scheduler: ' + any_scheduler)
print('any_iter: ' + any_iter)
print('any_id: ' + any_id)
print('show_all: ' + show_all)
print()

file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + any_id + '.txt'
f = open(file_name, 'r', errors='ignore')

line = f.readline()
while line:
    if len(line) > 1:
        line = line.replace('\n', '')
        line_s = line.split('] ')
        if len(line_s) > 1:
            message = line_s[1].split(' ')
            if message[0] == 'HCK-NODE':
                if message[1] == 'root':
                    ROOT_ID = message[2]
                    ROOT_ADDR = message[3]
                    all_id_list.append(int(message[2]))
                elif message[1] == 'non_root':
                    non_root_id_list.append(int(message[2]))
                    non_root_address_list.append(message[3])
                    all_id_list.append(int(message[2]))
                elif message[1] == 'end':
                    break
    line = f.readline()
f.close()

NODE_NUM = 1 + len(non_root_id_list) # root + non-root nodes

# STEP-1-3: print node info (id and address)
print('----- node info -----')
print('root id: ', ROOT_ID)
#print('root addr: ', ROOT_ADDR)
print('non-root ids: ', non_root_id_list)
#print('non-root addr:', non_root_address_list)
print('all id: ', all_id_list)
print()



########## STEP-2: preparation for parsing ##########

rx_tx_matrix = [[0 for i in range(NODE_NUM)] for j in range(NODE_NUM)]

for rx_id in all_id_list:
    rx_index = all_id_list.index(rx_id)

    file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + str(rx_id) + '.txt'
    f = open(file_name, 'r', errors='ignore')

    line = f.readline()
    while line:
        if len(line) > 1:
            line = line.replace('\n', '')
            line_s = line.split('rx LL-')
            if len(line_s) > 1:
                message = line_s[1].split('-')
                tx_id = int(message[0], 16)
                tx_index = tx_id - 1
                rx_tx_matrix[rx_index][tx_index] = 1
        line = f.readline()
    f.close()

nbr_count = [0 for i in range(NODE_NUM)]
for i in range(NODE_NUM):
    nbr_count[i] = sum(rx_tx_matrix[i])

for i in range(NODE_NUM):
    print(nbr_count[i])
