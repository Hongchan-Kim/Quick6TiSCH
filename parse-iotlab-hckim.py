import argparse

# STEP-1-1: variables and lists for node info (id and address)
ROOT_ID = 0
ROOT_ADDR = 'NULL'
non_root_id_list = list()
non_root_address_list = list()

# STEP-1-2: extract node info (id and address)
parser = argparse.ArgumentParser()
parser.add_argument('iter')
parser.add_argument('any_id')
args = parser.parse_args()

iter = args.iter
any_id = args.any_id

print('iter: ' + iter)
print('any_id: ' + any_id)

file_name = 'log-' + iter + '-' + any_id + '.txt'
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
                    ROOT_ID = message[2]
                    ROOT_ADDR = message[3]
                elif message[1] == 'non_root':
                    non_root_id_list.append(int(message[2]))
                    non_root_address_list.append(message[3])
                elif message[1] == 'end':
                    break
    line = f.readline()
f.close()

# STEP-1-3: print node info (id and address)
print('root id: ', ROOT_ID)
print('root addr: ', ROOT_ADDR)
print('non-root ids: ', non_root_id_list)
print('non-root addr:', non_root_address_list)


# STEP-2-1: metric indicators
metric_list = ['id', 'addr', 
            'tx_up', 'rx_up', 'tx_down', 'rx_down', 
            'fwd', 'ip_uc_tx', 'ip_uc_ok', 'ip_uc_noack', 'ip_uc_err',
            'ka_send', 'ka_qloss', 'ka_enqueue', 'ka_tx', 'ka_ok', 'ka_noack', 'ka_err',
            'eb_send', 'eb_qloss', 'eb_enqueue', 'eb_ok', 'eb_noack', 'eb_err',
            'ip_qloss', 'ip_enqueue', 'ip_ok', 'ip_noack', 'ip_err', 
            'asso', 'leaving', 'leave_time',
            'act_ts', 'sch_ts', 'ass_ts',
            'dis_send', 'dioU_send', 'dioM_send', 'daoP_send', 'daoN_send', 'daoA_send',
            'ps', 'lastP', 'local_repair',
            'hopD_sum', 'hopD_cnt', 'rdt',
            'subtree_sum', 'subtree_cnt',
            'dc']

# STEP-2-2: location of metric indicator and value
HCK = 0
IND = 1
VAL = 2
NID = 4
ADDR = 5

# STEP-2-3: parsed array
NODE_NUM = 1 + len(non_root_id_list) # root + non-root nodes
METRIC_NUM = len(metric_list)
parsed = [[0 for i in range(METRIC_NUM)] for j in range(NODE_NUM)]

# STEP-2-4: parse non-root nodes first
for node_id in non_root_id_list:
    node_index = non_root_id_list.index(node_id) + 1 # index in parsed array
    parsed[node_index][metric_list.index('id')] = node_id
    parsed[node_index][metric_list.index('addr')] = non_root_address_list[non_root_id_list.index(node_id)]

    file_name = 'log-' + iter + '-' + str(node_id) + '.txt'
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
                                parsed[node_index][current_metric_index] = 0
                            else:
                                if message[VAL] == ROOT_ADDR:
                                    parsed[node_index][current_metric_index] = ROOT_ID
                                else:
                                    parsed[node_index][current_metric_index] = non_root_id_list[non_root_address_list.index(message[VAL])]
                        else:
                            parsed[node_index][current_metric_index] = message[VAL]
        line = f.readline()
    f.close()

# STEP-2-5: parse root node
ROOT_INDEX = 0
parsed[ROOT_INDEX][metric_list.index('id')] = ROOT_ID
parsed[ROOT_INDEX][metric_list.index('addr')] = ROOT_ADDR

file_name = 'log-' + iter + '-' + str(ROOT_ID) + '.txt'
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
                    if current_metric == 'rx_up':
                        tx_node_index = non_root_address_list.index(message[ADDR]) + 1 # index in parsed array
                        parsed[tx_node_index][current_metric_index] = message[VAL]
                    elif current_metric == 'tx_down':
                        rx_node_index = non_root_address_list.index(message[ADDR]) + 1 # index in parsed array
                        parsed[rx_node_index][current_metric_index] = message[VAL]
                    else:
                        parsed[ROOT_INDEX][current_metric_index] = message[VAL]
    line = f.readline()
f.close()

# STEP-2-6: print parsed log
'''
for i in range(METRIC_NUM - 1):
    print(metric_list[i], '\t', end='')
print(metric_list[-1])
for i in range(NODE_NUM):
    for j in range(METRIC_NUM - 1):
        print(parsed[i][j], '\t', end='')
    print(parsed[i][-1])
'''


# STEP-3-1: result list and array
result_list = ['id', 'addr', 'tx_up', 'rx_up', 'uPdr', 'tx_dw', 'rx_dw', 'dPdr', 'pdr', 
               'lastP', 'ps', 'hopD', 'subTN', 'QLR', 'LLR', 'linkE', 'leave', 'SATR', 'ASTR', 'AATR', 'dc']
RESULT_NUM = len(result_list)
result = [[0 for i in range(RESULT_NUM)] for j in range(NODE_NUM)]

for i in range(NODE_NUM):
    for j in range(RESULT_NUM):
        if result_list[j] == 'id':
            result[i][j] = parsed[i][metric_list.index('id')]
        elif result_list[j] == 'addr':
            result[i][j] = parsed[i][metric_list.index('addr')]
        elif result_list[j] == 'tx_up':
            result[i][j] = parsed[i][metric_list.index('tx_up')]
        elif result_list[j] == 'rx_up':
            result[i][j] = parsed[i][metric_list.index('rx_up')]
        elif result_list[j] == 'uPdr':
            if i == ROOT_INDEX or int(parsed[i][metric_list.index('tx_up')]) == 0:
                result[i][j] = 'INF'
            else:
                result[i][j] = round(float(parsed[i][metric_list.index('rx_up')]) / float(parsed[i][metric_list.index('tx_up')]) * 100, 2)
        elif result_list[j] == 'tx_dw':
            result[i][j] = parsed[i][metric_list.index('tx_down')]
        elif result_list[j] == 'rx_dw':
            result[i][j] = parsed[i][metric_list.index('rx_down')]
        elif result_list[j] == 'dPdr':
            if i == ROOT_INDEX or int(parsed[i][metric_list.index('tx_down')]) == 0:
                result[i][j] = 'INF'
            else:
                result[i][j] = round(float(parsed[i][metric_list.index('rx_down')]) / float(parsed[i][metric_list.index('tx_down')]) * 100, 2)
        elif result_list[j] == 'pdr':
            if i == ROOT_INDEX or int(parsed[i][metric_list.index('tx_up')]) + int(parsed[i][metric_list.index('tx_down')]) == 0:
                result[i][j] = 'INF'
            else:
                result[i][j] = round((float(parsed[i][metric_list.index('rx_up')]) + float(parsed[i][metric_list.index('rx_down')])) / (float(parsed[i][metric_list.index('tx_up')]) + float(parsed[i][metric_list.index('tx_down')])) * 100, 2)
        elif result_list[j] == 'lastP':
            result[i][j] = parsed[i][metric_list.index('lastP')]
        elif result_list[j] == 'ps':
            result[i][j] = parsed[i][metric_list.index('ps')]
        elif result_list[j] == 'hopD':
            if int(parsed[i][metric_list.index('hopD_cnt')]) == 0:
                result[i][j] = 'INF'
            else:
                result[i][j] = round(float(parsed[i][metric_list.index('hopD_sum')]) / float(parsed[i][metric_list.index('hopD_cnt')]), 2)
        elif result_list[j] == 'subTN':
            if int(parsed[i][metric_list.index('subtree_cnt')]) == 0:
                result[i][j] = 'INF'
            else:
                result[i][j] = round(float(parsed[i][metric_list.index('subtree_sum')]) / float(parsed[i][metric_list.index('subtree_cnt')]), 2)
        elif result_list[j] == 'QLR':
            tot_qloss = float(parsed[i][metric_list.index('ip_qloss')]) + float(parsed[i][metric_list.index('ka_qloss')]) + float(parsed[i][metric_list.index('eb_qloss')])
            tot_enqueue = float(parsed[i][metric_list.index('ip_enqueue')]) + float(parsed[i][metric_list.index('ka_enqueue')]) + float(parsed[i][metric_list.index('eb_enqueue')])
            if tot_qloss + tot_enqueue == 0:
                result[i][j] = 'INF'
            else:
                result[i][j] = round(float(tot_qloss) / (float(tot_qloss) + float(tot_enqueue)) * 100, 2)
        elif result_list[j] == 'LLR':
            tot_uc_noack = float(parsed[i][metric_list.index('ip_uc_noack')]) + float(parsed[i][metric_list.index('ka_noack')])
            tot_uc_ok = float(parsed[i][metric_list.index('ip_uc_ok')]) + float(parsed[i][metric_list.index('ka_ok')])
            if tot_uc_noack + tot_uc_ok == 0:
                result[i][j] = 'INF'
            else:
                result[i][j] = round(float(tot_uc_noack) / (float(tot_uc_noack) + float(tot_uc_ok)) * 100, 2)
        elif result_list[j] == 'linkE':
            tot_uc_tx = float(parsed[i][metric_list.index('ka_tx')]) + float(parsed[i][metric_list.index('ip_uc_tx')])
            tot_uc_ok = float(parsed[i][metric_list.index('ka_ok')]) + float(parsed[i][metric_list.index('ip_uc_ok')])
            if tot_uc_ok == 0:
                result[i][j] = 'INF'
            else:
                result[i][j] = round(float(tot_uc_tx) / float(tot_uc_ok), 2)
        elif result_list[j] == 'leave':
            result[i][j] = parsed[i][metric_list.index('leaving')]
        elif result_list[j] == 'actTS':
            result[i][j] = int(parsed[i][metric_list.index('act_ts')])
        elif result_list[j] == 'schTS':
            result[i][j] = int(parsed[i][metric_list.index('sch_ts')])
        elif result_list[j] == 'assTS':
            result[i][j] = int(parsed[i][metric_list.index('ass_ts')])
        elif result_list[j] == 'SATR':
            if float(parsed[i][metric_list.index('ass_ts')]) == 0:
                result[i][j] = 'INF'
            else:
                result[i][j] = round(float(parsed[i][metric_list.index('sch_ts')]) / float(parsed[i][metric_list.index('ass_ts')]) * 100, 2)
        elif result_list[j] == 'ASTR':
            if float(parsed[i][metric_list.index('sch_ts')]) == 0:
                result[i][j] = 'INF'
            else:
                result[i][j] = round(float(parsed[i][metric_list.index('act_ts')]) / float(parsed[i][metric_list.index('sch_ts')]) * 100, 2)
        elif result_list[j] == 'AATR':
            if float(parsed[i][metric_list.index('ass_ts')]) == 0:
                result[i][j] = 'INF'
            else:
                result[i][j] = round(float(parsed[i][metric_list.index('act_ts')]) / float(parsed[i][metric_list.index('ass_ts')]) * 100, 2)
        elif result_list[j] == 'dc':
            result[i][j] = float(parsed[i][metric_list.index('dc')]) / 10
        
# STEP-3-2: print result
for i in range(RESULT_NUM - 1):
    print(result_list[i], '\t', end='')
print(result_list[-1])
for i in range(NODE_NUM):
    for j in range(RESULT_NUM - 1):
        print(result[i][j], '\t', end='')
    print(result[i][-1])