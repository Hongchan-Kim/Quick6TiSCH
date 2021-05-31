import argparse

########## STEP-1: collect node information ##########

# STEP-1-1: declare variables and lists for node info (id and address)
ROOT_ID = 1
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

print('----- evaluation info -----')
print('iter: ' + iter)
print('any_id: ' + any_id)
print()

file_name = 'log-' + iter + '-' + any_id + '.txt'
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
                elif message[1] == 'non_root':
                    non_root_id_list.append(int(message[2]))
                    non_root_address_list.append(message[3])
                elif message[1] == 'end':
                    break
    line = f.readline()
f.close()

NODE_NUM = 1 + len(non_root_id_list) # root + non-root nodes

# STEP-1-3: print node info (id and address)
print('----- node info -----')
print('root id: ', ROOT_ID)
print('root addr: ', ROOT_ADDR)
print('non-root ids: ', non_root_id_list)
print('non-root addr:', non_root_address_list)
print()



########## STEP-2: preparation for parsing ##########

# STEP-2-1: define location of metric indicator and value
HCK = 0
IND = 1
VAL = 2
NID = 4
ADDR = 5

# STEP-2-2: define metric indicators
metric_list = ['id', 'addr', 
            'tx_up', 'rx_up', 'tx_down', 'rx_down', 
            'fwd_ok', 'fwd_no_nexthop', 'fwd_err',
            'ip_uc_tx', 'ip_uc_ok', 'ip_uc_noack', 'ip_uc_err',
            'ka_send', 'ka_qloss', 'ka_enqueue', 'ka_tx', 'ka_ok', 'ka_noack', 'ka_err',
            'eb_send', 'eb_qloss', 'eb_enqueue', 'eb_ok', 'eb_noack', 'eb_err',
            'ip_qloss', 'ip_enqueue', 'ip_ok', 'ip_noack', 'ip_err', 
            'input_qloss',
            'asso', 'leaving', 'leave_time',
            'act_ts', 'sch_ts', 'ass_ts',
            'dis_send', 'dioU_send', 'dioM_send',
            'daoP_send', 'daoN_send', 'daoP_fwd', 'daoN_fwd', 'daoA_send',
            'ps', 'lastP', 'local_repair',
            'hopD_now', 'hopD_sum', 'hopD_cnt', 'rdt',
            'subtree_now', 'subtree_sum', 'subtree_cnt',
            'dc_count', 'dc_tx_sum', 'dc_rx_sum', 'dc_total_sum',
            'ostP_act_ts', 'ostP_sch_ts', 'ostO_act_ts', 'ostO_sch_ts']
METRIC_NUM = len(metric_list)

# STEP-2-3: define metric indicators to preserve
preserve_metric_list = ['id', 'addr',
                    'tx_up', 'rx_up', 'tx_down', 'rx_down', 
                    'lastP']
PRESERVE_METRIC_NUM = len(preserve_metric_list)

# STEP-2-4: define result list
result_list = ['id', 'addr', 'tx_up', 'rx_up', 'uPdr', 'tx_dw', 'rx_dw', 'dPdr', 'pdr', 
               'lastP', 'ps', 'hopD', 'aHopD', 'STN', 'aSTN', 'IPQL', 'IPQR', 'IPLL', 'IPLR', 'InQL', 'linkE', 'leave', 'SATR', 'ASTR', 'AATR', 'dc']
RESULT_NUM = len(result_list)




########## STEP-3: parse BOOTSTRAP PERIOD data ##########

# STEP-3-1: [bootstrap period] declare bootstrap_period_parsed array
bootstrap_period_parsed = [[0 for i in range(METRIC_NUM)] for j in range(NODE_NUM)]
bootstrap_period_finished_any = 0

# STEP-3-2: [bootstrap period] parse non-root nodes first
for node_id in non_root_id_list:
    node_index = non_root_id_list.index(node_id) + 1 # index in bootstrap_period_parsed array
    bootstrap_period_parsed[node_index][metric_list.index('id')] = node_id
    bootstrap_period_parsed[node_index][metric_list.index('addr')] = non_root_address_list[non_root_id_list.index(node_id)]

    file_name = 'log-' + iter + '-' + str(node_id) + '.txt'
    f = open(file_name, 'r', errors='ignore')

    line = f.readline()
    while line:
        if len(line) > 1:
            line = line.replace('\n', '')
            line_s = line.split('] ')
            if len(line_s) > 1:
                message = line_s[1].split(' ')
                if message[HCK] == 'HCK':
                    current_metric = message[IND]
                    if current_metric == 'reset_log':
                        bootstrap_period_finished_any = 1
                        break
                    elif current_metric in metric_list:
                        current_metric_index = metric_list.index(current_metric)
                        if current_metric == 'lastP': # to find node_id of last parent 
                            if message[VAL] == '0':
                                bootstrap_period_parsed[node_index][current_metric_index] = 0
                            else:
                                if message[VAL] == ROOT_ADDR:
                                    bootstrap_period_parsed[node_index][current_metric_index] = ROOT_ID
                                else:
                                    bootstrap_period_parsed[node_index][current_metric_index] = non_root_id_list[non_root_address_list.index(message[VAL])]
                        else:
                            bootstrap_period_parsed[node_index][current_metric_index] = message[VAL]
        line = f.readline()
    f.close()

# STEP-3-3: [bootstrap period] parse root node
ROOT_INDEX = 0
bootstrap_period_parsed[ROOT_INDEX][metric_list.index('id')] = ROOT_ID
bootstrap_period_parsed[ROOT_INDEX][metric_list.index('addr')] = ROOT_ADDR

file_name = 'log-' + iter + '-' + str(ROOT_ID) + '.txt'
f = open(file_name, 'r', errors='ignore')

line = f.readline()
while line:
    if len(line) > 1:
        line = line.replace('\n', '')
        line_s = line.split('] ')
        if len(line_s) > 1:
            message = line_s[1].split(' ')
            if message[HCK] == 'HCK':
                current_metric = message[IND]
                if current_metric == 'reset_log':
                    bootstrap_period_finished_any = 1
                    break
                elif current_metric in metric_list:
                    current_metric_index = metric_list.index(current_metric)
                    if current_metric == 'rx_up':
                        tx_node_index = non_root_address_list.index(message[ADDR]) + 1 # index in bootstrap_period_parsed array
                        bootstrap_period_parsed[tx_node_index][current_metric_index] = message[VAL]
                    elif current_metric == 'tx_down':
                        rx_node_index = non_root_address_list.index(message[ADDR]) + 1 # index in bootstrap_period_parsed array
                        bootstrap_period_parsed[rx_node_index][current_metric_index] = message[VAL]
                    else:
                        bootstrap_period_parsed[ROOT_INDEX][current_metric_index] = message[VAL]
    line = f.readline()
f.close()

# STEP-3-4: [bootstrap period] generate bootstrap_period_result
bootstrap_period_result = [[0 for i in range(RESULT_NUM)] for j in range(NODE_NUM)]

for i in range(NODE_NUM):
    for j in range(RESULT_NUM):
        if result_list[j] == 'id':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('id')]
        elif result_list[j] == 'addr':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('addr')]
        elif result_list[j] == 'tx_up':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('tx_up')]
        elif result_list[j] == 'rx_up':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('rx_up')]
        elif result_list[j] == 'uPdr':
            if i == ROOT_INDEX or int(bootstrap_period_parsed[i][metric_list.index('tx_up')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('rx_up')]) / float(bootstrap_period_parsed[i][metric_list.index('tx_up')]) * 100, 2)
        elif result_list[j] == 'tx_dw':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('tx_down')]
        elif result_list[j] == 'rx_dw':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('rx_down')]
        elif result_list[j] == 'dPdr':
            if i == ROOT_INDEX or int(bootstrap_period_parsed[i][metric_list.index('tx_down')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('rx_down')]) / float(bootstrap_period_parsed[i][metric_list.index('tx_down')]) * 100, 2)
        elif result_list[j] == 'pdr':
            if i == ROOT_INDEX or int(bootstrap_period_parsed[i][metric_list.index('tx_up')]) + int(bootstrap_period_parsed[i][metric_list.index('tx_down')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round((float(bootstrap_period_parsed[i][metric_list.index('rx_up')]) + float(bootstrap_period_parsed[i][metric_list.index('rx_down')])) / (float(bootstrap_period_parsed[i][metric_list.index('tx_up')]) + float(bootstrap_period_parsed[i][metric_list.index('tx_down')])) * 100, 2)
        elif result_list[j] == 'lastP':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('lastP')]
        elif result_list[j] == 'ps':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('ps')]
        elif result_list[j] == 'hopD':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('hopD_now')]
        elif result_list[j] == 'aHopD':
            if int(bootstrap_period_parsed[i][metric_list.index('hopD_cnt')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('hopD_sum')]) / float(bootstrap_period_parsed[i][metric_list.index('hopD_cnt')]), 2)
        elif result_list[j] == 'STN':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('subtree_now')]
        elif result_list[j] == 'aSTN':
            if int(bootstrap_period_parsed[i][metric_list.index('subtree_cnt')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('subtree_sum')]) / float(bootstrap_period_parsed[i][metric_list.index('subtree_cnt')]), 2)
        elif result_list[j] == 'IPQL':
            tot_qloss = int(bootstrap_period_parsed[i][metric_list.index('ip_qloss')])# + int(bootstrap_period_parsed[i][metric_list.index('ka_qloss')]) + int(bootstrap_period_parsed[i][metric_list.index('eb_qloss')])
            bootstrap_period_result[i][j] = tot_qloss
        elif result_list[j] == 'IPQR':
            tot_qloss = float(bootstrap_period_parsed[i][metric_list.index('ip_qloss')])# + float(bootstrap_period_parsed[i][metric_list.index('ka_qloss')]) + float(bootstrap_period_parsed[i][metric_list.index('eb_qloss')])
            tot_enqueue = float(bootstrap_period_parsed[i][metric_list.index('ip_enqueue')])# + float(bootstrap_period_parsed[i][metric_list.index('ka_enqueue')]) + float(bootstrap_period_parsed[i][metric_list.index('eb_enqueue')])
            if tot_qloss + tot_enqueue == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(tot_qloss) / (float(tot_qloss) + float(tot_enqueue)) * 100, 2)
        elif result_list[j] == 'IPLL':
            tot_uc_noack = int(bootstrap_period_parsed[i][metric_list.index('ip_uc_noack')])# + int(bootstrap_period_parsed[i][metric_list.index('ka_noack')])
            bootstrap_period_result[i][j] = tot_uc_noack
        elif result_list[j] == 'IPLR':
            tot_uc_noack = float(bootstrap_period_parsed[i][metric_list.index('ip_uc_noack')])# + float(bootstrap_period_parsed[i][metric_list.index('ka_noack')])
            tot_uc_ok = float(bootstrap_period_parsed[i][metric_list.index('ip_uc_ok')])# + float(bootstrap_period_parsed[i][metric_list.index('ka_ok')])
            if tot_uc_noack + tot_uc_ok == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(tot_uc_noack) / (float(tot_uc_noack) + float(tot_uc_ok)) * 100, 2)
        elif result_list[j] == 'InQL':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('input_qloss')]
        elif result_list[j] == 'linkE':
            tot_uc_tx = float(bootstrap_period_parsed[i][metric_list.index('ka_tx')]) + float(bootstrap_period_parsed[i][metric_list.index('ip_uc_tx')])
            tot_uc_ok = float(bootstrap_period_parsed[i][metric_list.index('ka_ok')]) + float(bootstrap_period_parsed[i][metric_list.index('ip_uc_ok')])
            if tot_uc_ok == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(tot_uc_tx) / float(tot_uc_ok), 2)
        elif result_list[j] == 'leave':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('leaving')]
        elif result_list[j] == 'actTS':
            bootstrap_period_result[i][j] = int(bootstrap_period_parsed[i][metric_list.index('act_ts')])
        elif result_list[j] == 'schTS':
            bootstrap_period_result[i][j] = int(bootstrap_period_parsed[i][metric_list.index('sch_ts')])
        elif result_list[j] == 'assTS':
            bootstrap_period_result[i][j] = int(bootstrap_period_parsed[i][metric_list.index('ass_ts')])
        elif result_list[j] == 'SATR':
            if float(bootstrap_period_parsed[i][metric_list.index('ass_ts')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('sch_ts')]) / float(bootstrap_period_parsed[i][metric_list.index('ass_ts')]) * 100, 2)
        elif result_list[j] == 'ASTR':
            if float(bootstrap_period_parsed[i][metric_list.index('sch_ts')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('act_ts')]) / float(bootstrap_period_parsed[i][metric_list.index('sch_ts')]) * 100, 2)
        elif result_list[j] == 'AATR':
            if float(bootstrap_period_parsed[i][metric_list.index('ass_ts')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('act_ts')]) / float(bootstrap_period_parsed[i][metric_list.index('ass_ts')]) * 100, 2)
        elif result_list[j] == 'dc':
            if float(bootstrap_period_parsed[i][metric_list.index('dc_count')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('dc_total_sum')]) / float(bootstrap_period_parsed[i][metric_list.index('dc_count')]) / 100, 2)
        
# STEP-3-5: [bootstrap period] print bootstrap_period_result
print('----- bootstrap period -----')
for i in range(RESULT_NUM - 1):
    print(result_list[i], '\t', end='')
print(result_list[-1])
for i in range(NODE_NUM):
    for j in range(RESULT_NUM - 1):
        print(bootstrap_period_result[i][j], '\t', end='')
    print(bootstrap_period_result[i][-1])
print()

if bootstrap_period_finished_any == 0:
    print('----- in bootstrap period -----')
    exit()

# STEP-4-1: [data period] declare data_period_parsed array
data_period_parsed = [[0 for i in range(METRIC_NUM)] for j in range(NODE_NUM)]

# STEP-4-2: [data period] copy metric must be preserved from bootstrap_period_parsed
for i in range(PRESERVE_METRIC_NUM):
    preserve_metric = preserve_metric_list[i]
    preserve_metric_index = metric_list.index(preserve_metric) # index in metric_list
    for j in range(NODE_NUM):
        data_period_parsed[j][preserve_metric_index] = bootstrap_period_parsed[j][preserve_metric_index]

# STEP-4-3: [data period] parse non-root nodes first
for node_id in non_root_id_list:
    node_index = non_root_id_list.index(node_id) + 1 # index in parsed array

    file_name = 'log-' + iter + '-' + str(node_id) + '.txt'
    f = open(file_name, 'r', errors='ignore')

    in_data_period = 0

    line = f.readline()
    while line:
        if len(line) > 1:
            line = line.replace('\n', '')
            line_s = line.split('] ')
            if len(line_s) > 1:
                message = line_s[1].split(' ')
                if message[HCK] == 'HCK':
                    current_metric = message[IND]
                    if current_metric == 'reset_log':
                        in_data_period = 1
                    if in_data_period == 1:
                        if current_metric in metric_list:
                            current_metric_index = metric_list.index(current_metric)
                            if current_metric == 'lastP': # to find node_id of last parent 
                                if message[VAL] == '0':
                                    data_period_parsed[node_index][current_metric_index] = 0
                                else:
                                    if message[VAL] == ROOT_ADDR:
                                        data_period_parsed[node_index][current_metric_index] = ROOT_ID
                                    else:
                                        data_period_parsed[node_index][current_metric_index] = non_root_id_list[non_root_address_list.index(message[VAL])]
                            else:
                                data_period_parsed[node_index][current_metric_index] = message[VAL]
        line = f.readline()
    f.close()

# STEP-4-4: [data period] parse root node
ROOT_INDEX = 0
data_period_parsed[ROOT_INDEX][metric_list.index('id')] = ROOT_ID
data_period_parsed[ROOT_INDEX][metric_list.index('addr')] = ROOT_ADDR

file_name = 'log-' + iter + '-' + str(ROOT_ID) + '.txt'
f = open(file_name, 'r', errors='ignore')

in_data_period = 0

line = f.readline()
while line:
    if len(line) > 1:
        line = line.replace('\n', '')
        line_s = line.split('] ')
        if len(line_s) > 1:
            message = line_s[1].split(' ')
            if message[HCK] == 'HCK':
                current_metric = message[IND]
                if current_metric == 'reset_log':
                    in_data_period = 1
                if in_data_period == 1:
                    if current_metric in metric_list:
                        current_metric_index = metric_list.index(current_metric)
                        if current_metric == 'rx_up':
                            tx_node_index = non_root_address_list.index(message[ADDR]) + 1 # index in data_period_parsed array
                            data_period_parsed[tx_node_index][current_metric_index] = message[VAL]
                        elif current_metric == 'tx_down':
                            rx_node_index = non_root_address_list.index(message[ADDR]) + 1 # index in data_period_parsed array
                            data_period_parsed[rx_node_index][current_metric_index] = message[VAL]
                        else:
                            data_period_parsed[ROOT_INDEX][current_metric_index] = message[VAL]
    line = f.readline()
f.close()

# STEP-4-5: [data period] generate data_period_result
data_period_result = [[0 for i in range(RESULT_NUM)] for j in range(NODE_NUM)]

for i in range(NODE_NUM):
    for j in range(RESULT_NUM):
        if result_list[j] == 'id':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('id')]
        elif result_list[j] == 'addr':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('addr')]
        elif result_list[j] == 'tx_up':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('tx_up')]
        elif result_list[j] == 'rx_up':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('rx_up')]
        elif result_list[j] == 'uPdr':
            if i == ROOT_INDEX or int(data_period_parsed[i][metric_list.index('tx_up')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('rx_up')]) / float(data_period_parsed[i][metric_list.index('tx_up')]) * 100, 2)
        elif result_list[j] == 'tx_dw':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('tx_down')]
        elif result_list[j] == 'rx_dw':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('rx_down')]
        elif result_list[j] == 'dPdr':
            if i == ROOT_INDEX or int(data_period_parsed[i][metric_list.index('tx_down')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('rx_down')]) / float(data_period_parsed[i][metric_list.index('tx_down')]) * 100, 2)
        elif result_list[j] == 'pdr':
            if i == ROOT_INDEX or int(data_period_parsed[i][metric_list.index('tx_up')]) + int(data_period_parsed[i][metric_list.index('tx_down')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round((float(data_period_parsed[i][metric_list.index('rx_up')]) + float(data_period_parsed[i][metric_list.index('rx_down')])) / (float(data_period_parsed[i][metric_list.index('tx_up')]) + float(data_period_parsed[i][metric_list.index('tx_down')])) * 100, 2)
        elif result_list[j] == 'lastP':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('lastP')]
        elif result_list[j] == 'ps':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('ps')]
        elif result_list[j] == 'hopD':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('hopD_now')]
        elif result_list[j] == 'aHopD':
            if int(data_period_parsed[i][metric_list.index('hopD_cnt')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('hopD_sum')]) / float(data_period_parsed[i][metric_list.index('hopD_cnt')]), 2)
        elif result_list[j] == 'STN':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('subtree_now')]
        elif result_list[j] == 'aSTN':
            if int(data_period_parsed[i][metric_list.index('subtree_cnt')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('subtree_sum')]) / float(data_period_parsed[i][metric_list.index('subtree_cnt')]), 2)
        elif result_list[j] == 'IPQL':
            tot_qloss = int(data_period_parsed[i][metric_list.index('ip_qloss')])# + int(data_period_parsed[i][metric_list.index('ka_qloss')]) + int(data_period_parsed[i][metric_list.index('eb_qloss')])
            data_period_result[i][j] = tot_qloss
        elif result_list[j] == 'IPQR':
            tot_qloss = float(data_period_parsed[i][metric_list.index('ip_qloss')])# + float(data_period_parsed[i][metric_list.index('ka_qloss')]) + float(data_period_parsed[i][metric_list.index('eb_qloss')])
            tot_enqueue = float(data_period_parsed[i][metric_list.index('ip_enqueue')])# + float(data_period_parsed[i][metric_list.index('ka_enqueue')]) + float(data_period_parsed[i][metric_list.index('eb_enqueue')])
            if tot_qloss + tot_enqueue == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(tot_qloss) / (float(tot_qloss) + float(tot_enqueue)) * 100, 2)
        elif result_list[j] == 'IPLL':
            tot_uc_noack = int(data_period_parsed[i][metric_list.index('ip_uc_noack')])# + int(data_period_parsed[i][metric_list.index('ka_noack')])
            data_period_result[i][j] = tot_uc_noack
        elif result_list[j] == 'IPLR':
            tot_uc_noack = float(data_period_parsed[i][metric_list.index('ip_uc_noack')])# + float(data_period_parsed[i][metric_list.index('ka_noack')])
            tot_uc_ok = float(data_period_parsed[i][metric_list.index('ip_uc_ok')])# + float(data_period_parsed[i][metric_list.index('ka_ok')])
            if tot_uc_noack + tot_uc_ok == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(tot_uc_noack) / (float(tot_uc_noack) + float(tot_uc_ok)) * 100, 2)
        elif result_list[j] == 'InQL':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('input_qloss')]
        elif result_list[j] == 'linkE':
            tot_uc_tx = float(data_period_parsed[i][metric_list.index('ka_tx')]) + float(data_period_parsed[i][metric_list.index('ip_uc_tx')])
            tot_uc_ok = float(data_period_parsed[i][metric_list.index('ka_ok')]) + float(data_period_parsed[i][metric_list.index('ip_uc_ok')])
            if tot_uc_ok == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(tot_uc_tx) / float(tot_uc_ok), 2)
        elif result_list[j] == 'leave':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('leaving')]
        elif result_list[j] == 'actTS':
            data_period_result[i][j] = int(data_period_parsed[i][metric_list.index('act_ts')])
        elif result_list[j] == 'schTS':
            data_period_result[i][j] = int(data_period_parsed[i][metric_list.index('sch_ts')])
        elif result_list[j] == 'assTS':
            data_period_result[i][j] = int(data_period_parsed[i][metric_list.index('ass_ts')])
        elif result_list[j] == 'SATR':
            if float(data_period_parsed[i][metric_list.index('ass_ts')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('sch_ts')]) / float(data_period_parsed[i][metric_list.index('ass_ts')]) * 100, 2)
        elif result_list[j] == 'ASTR':
            if float(data_period_parsed[i][metric_list.index('sch_ts')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('act_ts')]) / float(data_period_parsed[i][metric_list.index('sch_ts')]) * 100, 2)
        elif result_list[j] == 'AATR':
            if float(data_period_parsed[i][metric_list.index('ass_ts')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('act_ts')]) / float(data_period_parsed[i][metric_list.index('ass_ts')]) * 100, 2)
        elif result_list[j] == 'dc':
            if float(data_period_parsed[i][metric_list.index('dc_count')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('dc_total_sum')]) / float(data_period_parsed[i][metric_list.index('dc_count')]) / 100, 2)


# STEP-4-6: [data period] print data_period_result
print('----- data period -----')
for i in range(RESULT_NUM - 1):
    print(result_list[i], '\t', end='')
print(result_list[-1])
for i in range(NODE_NUM):
    for j in range(RESULT_NUM - 1):
        print(data_period_result[i][j], '\t', end='')
    print(data_period_result[i][-1])