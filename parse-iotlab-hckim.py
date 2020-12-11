### parsing configuration ###

# SET-1: node indexing
ROOT_ID = 7
non_root_list = [1, 2, 3, 4, 5, 6, 8, 10, 11, 12, 13, 14, 15, 16, 17, 18]

# SET-2: metric indicators
metric_list = ['id', 'addr', 'txu', 'rxu', 'txd', 'rxd', 'dis_o', 'dio_o', 'dao_o', 'daoA_o', 
            'ps', 'lastP', 'child', 'fwo', 'qloss', 'enq', 'EBql', 'EBenq', 'noack', 'ok', 'dc']

# SET-3: location of metric indicator and value location
HCK = 0
IND = 1
VAL = 2
NID = 4
ADDR = 5



### NO NEED TO REVISE FROM HERE ###


# STEP-1: set array
NODE_NUM = 1 + len(non_root_list) # root + non-root nodes
METRIC_NUM = len(metric_list)
address_list = [0 for i in range(NODE_NUM)]
result = [[0 for i in range(METRIC_NUM)] for j in range(NODE_NUM)]


# STEP-2: make address_list - non-root nodes
for node_id in non_root_list:
    file_name = 'log-' + str(node_id) + '.txt'
    f = open(file_name, 'r')

    node_index = non_root_list.index(node_id) + 1

    line = f.readline()
    while line:
        if len(line) > 1:
            line = line.replace('\n', '')
            line_s = line.split('] ')
            if len(line_s) > 1:
                message = line_s[1].split(' ')
                if message[0] == 'Tentative':
                    if message[1] == 'link-local':
                        address_list[node_index] = message[4].split(':')[2]
                        break
        line = f.readline()
    f.close()


# STEP-3: make address_list - root nodes
ROOT_INDEX = 0
file_name = 'log-' + str(ROOT_ID) + '.txt'
f = open(file_name, 'r')

line = f.readline()
while line:
    if len(line) > 1:
        line = line.replace('\n', '')
        line_s = line.split('] ')
        if len(line_s) > 1:
            message = line_s[1].split(' ')
            if message[0] == 'Tentative':
                if message[1] == 'link-local':
                    address_list[ROOT_INDEX] = message[4].split(':')[2]
                    break
    line = f.readline()
f.close()


# STEP-4: parse non-root nodes first
for node_id in non_root_list:
    node_index = non_root_list.index(node_id) + 1
    result[node_index][metric_list.index('id')] = node_id

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
                                if address_list.index(message[VAL]) == ROOT_INDEX:
                                    result[node_index][current_metric_index] = int(ROOT_ID)
                                else:
                                    result[node_index][current_metric_index] = int(non_root_list[address_list.index(message[VAL]) - 1])
                        else:
                            result[node_index][current_metric_index] = int(message[VAL])
        line = f.readline()
    f.close()


# STEP-5: parse root node
ROOT_INDEX = 0
result[ROOT_INDEX][metric_list.index('id')] = ROOT_ID

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
                        tx_node_index = address_list.index(message[ADDR])
                        result[tx_node_index][current_metric_index] = int(message[VAL])
                    elif current_metric == 'txd':
                        rx_node_index = address_list.index(message[ADDR])
                        result[rx_node_index][current_metric_index] = int(message[VAL])
                    else:
                        result[ROOT_INDEX][current_metric_index] = int(message[VAL])
    line = f.readline()
f.close()


# STEP-6: copy address list
for i in range(NODE_NUM):
    result[i][metric_list.index('addr')] = address_list[i]

# STEP-7: print parsed log
print('Root node: ', ROOT_ID)
for i in range(METRIC_NUM - 1):
    print(metric_list[i], '\t', end='')
print(metric_list[-1])
for i in range(NODE_NUM):
    for j in range(METRIC_NUM - 1):
        print(result[i][j], '\t', end='')
    print(result[i][-1])
