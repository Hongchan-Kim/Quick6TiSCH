import argparse

# STEP-0-1: variables and lists
ROOT_ID = 0
ROOT_ADDR = 'NULL'
non_root_id_list = list()
non_root_address_list = list()


# STEP-0-2: metric indicators
metric_list = ['id', 'addr', 'txu', 'rxu', 'txd', 'rxd', 'dis_o', 'dio_o', 'dao_o', 'daoA_o', 
            'ps', 'child', 'fwo', 'qloss', 'enq', 'EBql', 'EBenq', 'noack', 'ok', 'dc', 'lastP']


# STEP-0-3: location of metric indicator and value
HCK = 0
IND = 1
VAL = 2
NID = 4
ADDR = 5


# STEP-1: extract node info (id and address)
parser = argparse.ArgumentParser()
parser.add_argument('any_id', type=int)
args = parser.parse_args()

file_name = 'log-' + str(args.any_id) + '.txt'
f = open(file_name, 'r')

line = f.readline()
while line:
    if len(line) > 1:
        line = line.replace('\n', '')
        line_s = line.split('] ')
        if len(line_s) > 1:
            message = line_s[1].split(' ')
            if message[0] == 'HCK-NODE':
                if message[1] == 'root':
                    ROOT_ID = int(message[2])
                    ROOT_ADDR = message[3]
                elif message[1] == 'non':
                    non_root_id_list.append(int(message[2]))
                    non_root_address_list.append(message[3])
                elif message[1] == 'end':
                    break
    line = f.readline()
f.close()

print('root id: ', ROOT_ID)
print('root addr: ', ROOT_ADDR)
print('non-root ids: ', non_root_id_list)
print('non-root addr:', non_root_address_list)


# STEP-2: result array
NODE_NUM = 1 + len(non_root_id_list) # root + non-root nodes
METRIC_NUM = len(metric_list)
result = [[0 for i in range(METRIC_NUM)] for j in range(NODE_NUM)]


# STEP-3: parse non-root nodes first
for node_id in non_root_id_list:
    node_index = non_root_id_list.index(node_id) + 1 # index in result array
    result[node_index][metric_list.index('id')] = node_id
    result[node_index][metric_list.index('addr')] = non_root_address_list[non_root_id_list.index(node_id)]

    file_name = 'log-' + str(node_id) + '.txt'
    f = open(file_name, 'r')

    line = f.readline()
    while line:
        if len(line) > 1:
            line = line.replace('\n', '')
            line_s = line.split('] ')
            if len(line_s) > 1:
                message = line_s[1].split(' ')
                if message[HCK] == 'HCK':
                    current_metric = message[IND]
                    if current_metric in metric_list:
                        current_metric_index = metric_list.index(current_metric)
                        if current_metric == 'lastP': # to find node_id of last parent 
                            if message[VAL] == '0':
                                result[node_index][current_metric_index] = 0
                            else:
                                if message[VAL] == ROOT_ADDR:
                                    result[node_index][current_metric_index] = ROOT_ID
                                else:
                                    result[node_index][current_metric_index] = non_root_id_list[non_root_address_list.index(message[VAL])]
                        else:
                            result[node_index][current_metric_index] = message[VAL]
        line = f.readline()
    f.close()


# STEP-4: parse root node
ROOT_INDEX = 0
result[ROOT_INDEX][metric_list.index('id')] = ROOT_ID
result[ROOT_INDEX][metric_list.index('addr')] = ROOT_ADDR

file_name = 'log-' + str(ROOT_ID) + '.txt'
f = open(file_name, 'r')

line = f.readline()
while line:
    if len(line) > 1:
        line = line.replace('\n', '')
        line_s = line.split('] ')
        if len(line_s) > 1:
            message = line_s[1].split(' ')
            if message[HCK] == 'HCK':
                current_metric = message[IND]
                if current_metric in metric_list:
                    current_metric_index = metric_list.index(current_metric)
                    if current_metric == 'rxu':
                        tx_node_index = non_root_address_list.index(message[ADDR]) + 1 # index in result array
                        result[tx_node_index][current_metric_index] = int(message[VAL])
                    elif current_metric == 'txd':
                        rx_node_index = non_root_address_list.index(message[ADDR]) + 1 # index in result array
                        result[rx_node_index][current_metric_index] = int(message[VAL])
                    else:
                        result[ROOT_INDEX][current_metric_index] = int(message[VAL])
    line = f.readline()
f.close()


# STEP-5: print parsed log
for i in range(METRIC_NUM - 1):
    print(metric_list[i], '\t', end='')
print(metric_list[-1])
for i in range(NODE_NUM):
    for j in range(METRIC_NUM - 1):
        print(result[i][j], '\t', end='')
    print(result[i][-1])
