import argparse

########## STEP-1: collect node information ##########

# STEP-1-1: declare variables and lists for node info (id and address)
ROOT_ID = 1
ROOT_ADDR = 'NULL'
non_root_id_list = list()
non_root_address_list = list()

# STEP-1-2: extract node info (id and address)
parser = argparse.ArgumentParser()
parser.add_argument('any_iter')
parser.add_argument('any_id')
parser.add_argument('show_all')
args = parser.parse_args()

any_iter = args.any_iter
any_id = args.any_id
show_all = args.show_all

print('----- evaluation info -----')
print('any_iter: ' + any_iter)
print('any_id: ' + any_id)
print()

file_name = 'log-' + any_iter + '-' + any_id + '.txt'
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
            'tx_up', 'rx_up', 'lt_up_sum', 'tx_down', 'rx_down', 'lt_down_sum',
            'fwd_ok', 'fwd_no_nexthop', 'fwd_err',
            'ip_uc_tx', 'ip_uc_ok', 'ip_uc_noack', 'ip_uc_err',
            'ip_uc_icmp6_tx', 'ip_uc_icmp6_ok', 'ip_uc_icmp6_noack', 'ip_uc_icmp6_err',
            'ip_uc_udp_tx', 'ip_uc_udp_ok', 'ip_uc_udp_noack', 'ip_uc_udp_err',
            'asso', 'asso_ts', 'leaving', 'leave_time', 
            'eb_qloss', 'eb_enq', 'eb_ok', 'eb_noack', 'eb_err',
            'ka_send', 'ka_qloss', 'ka_enq', 'ka_tx', 'ka_ok', 'ka_noack', 'ka_err',
            'ip_qloss', 'ip_enq', 'ip_ok', 'ip_noack', 'ip_err', 
            'ip_icmp6_qloss', 'ip_icmp6_enq', 'ip_icmp6_ok', 'ip_icmp6_noack', 'ip_icmp6_err', 
            'ip_udp_qloss', 'ip_udp_enq', 'ip_udp_ok', 'ip_udp_noack', 'ip_udp_err', 
            'input_full', 'input_avail', 'dequeued_full', 'dequeued_avail', 'e_drop',
            'sch_eb', 'sch_bc', 'sch_uc', 'sch_pp_tx', 'sch_pp_rx', 'sch_odp_tx', 'sch_odp_rx',
            'sch_bc_bst_tx', 'sch_bc_bst_rx', 'sch_uc_bst_tx', 'sch_uc_bst_rx', 'sch_pp_bst_tx', 'sch_pp_bst_rx', 
            'sch_bc_ep_tx', 'sch_bc_ep_rx', 'sch_uc_ep_tx', 'sch_uc_ep_rx', 'sch_pp_ep_tx', 'sch_pp_ep_rx',  
            'eb_tx_op', 'eb_rx_op', 'bc_tx_op', 'bc_rx_op', 'uc_tx_op', 'uc_rx_op',
            'pp_tx_op', 'pp_rx_op', 'odp_tx_op', 'odp_rx_op',
            'bc_bst_tx_op', 'bc_bst_rx_op', 'uc_bst_tx_op', 'uc_bst_rx_op', 'pp_bst_tx_op', 'pp_bst_rx_op',
            'bc_ep_tx_rs', 'bc_ep_rx_rs', 'uc_ep_tx_rs', 'uc_ep_rx_rs', 'pp_ep_tx_rs', 'pp_ep_rx_rs',
            'bc_ep_tx_ok', 'bc_ep_rx_ok', 'uc_ep_tx_ok', 'uc_ep_rx_ok', 'pp_ep_tx_ok', 'pp_ep_rx_ok',
            'bc_ep_tx_ts', 'bc_ep_rx_ts', 'uc_ep_tx_ts', 'uc_ep_rx_ts', 'pp_ep_tx_ts', 'pp_ep_rx_ts',
            'ps', 'lastP', 'local_repair',
            'dis_send', 'dioU_send', 'dioM_send',
            'daoP_send', 'daoN_send', 'daoP_fwd', 'daoN_fwd', 'daoA_send',
            'hopD_now', 'hopD_sum', 'hopD_cnt', 'rdt',
            'subtree_now', 'subtree_sum', 'subtree_cnt',
            'dc_count', 'dc_tx_sum', 'dc_rx_sum', 'dc_total_sum']
METRIC_NUM = len(metric_list)

# STEP-2-3: define metric indicators to preserve
preserve_metric_list = ['id', 'addr',
                    'tx_up', 'rx_up', 'lt_up_sum', 'tx_down', 'rx_down', 'lt_down_sum',
                    'lastP']
PRESERVE_METRIC_NUM = len(preserve_metric_list)

# STEP-2-4: define result list
result_list = ['id', 'addr',
            'tx_up', 'rx_up', 'uPdr', 'tx_dw', 'rx_dw', 'dPdr', 'pdr', 'uLT', 'dLT', 'LT',
            'lastP', 'ps', 'hopD', 'aHopD', 'STN', 'aSTN', 
            'IPQL', 'IPQR', 'IPLL', 'IPLR', 'IUQL', 'IUQR', 'IULL', 'IULR', 'InQL', 'linkE', 
            'leave', 'dc', 'e_drop',
            'SCR', 'CTOR', 'CROR', 'UTOR', 'UROR', 'PTOR', 'PROR',
            'BTOR', 'BROR', 'OTOR', 'OROR', 'ETOR', 'EROR', 
            'CETR', 'CERR', 'UETR', 'UERR', 'PETR', 'PERR',
            'CETO', 'CERO', 'UETO', 'UERO', 'PETO', 'PERO',
            'CETE', 'CERE', 'UETE', 'UERE', 'PETE', 'PERE'
            ]
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

    file_name = 'log-' + any_iter + '-' + str(node_id) + '.txt'
    f = open(file_name, 'r', errors='ignore')
    bootstrap_period_finished_this = 0

    line = f.readline()
    while line:
        if len(line) > 1:
            line = line.replace('\n', '')
            line_s = line.split('] ')
            if len(line_s) > 1:
                message = line_s[1].split(' ')
                if message[HCK] == 'HCK':
                    ind_loc = IND
                    val_loc = VAL
                    while message[ind_loc] != '|':
                        current_metric = message[ind_loc]
                        if current_metric == 'reset_log':
                            bootstrap_period_finished_this = 1
                            bootstrap_period_finished_any = 1
                            break
                        elif current_metric in metric_list:
                            current_metric_index = metric_list.index(current_metric)
                            if current_metric == 'lastP': # to find node_id of last parent 
                                if message[val_loc] == '0':
                                    bootstrap_period_parsed[node_index][current_metric_index] = 0
                                else:
                                    if message[val_loc] == ROOT_ADDR:
                                        bootstrap_period_parsed[node_index][current_metric_index] = ROOT_ID
                                    else:
                                        bootstrap_period_parsed[node_index][current_metric_index] = non_root_id_list[non_root_address_list.index(message[val_loc])]
                            else:
                                bootstrap_period_parsed[node_index][current_metric_index] = message[val_loc]
                        ind_loc += 2
                        val_loc += 2
                        if ind_loc > len(message) or val_loc > len(message):
                            break
                    if bootstrap_period_finished_this == 1:
                        break
        line = f.readline()
    f.close()

# STEP-3-3: [bootstrap period] parse root node
ROOT_INDEX = 0
bootstrap_period_parsed[ROOT_INDEX][metric_list.index('id')] = ROOT_ID
bootstrap_period_parsed[ROOT_INDEX][metric_list.index('addr')] = ROOT_ADDR

file_name = 'log-' + any_iter + '-' + str(ROOT_ID) + '.txt'
f = open(file_name, 'r', errors='ignore')
bootstrap_period_finished_this = 0

line = f.readline()
while line:
    if len(line) > 1:
        line = line.replace('\n', '')
        line_s = line.split('] ')
        if len(line_s) > 1:
            message = line_s[1].split(' ')
            if message[HCK] == 'HCK':
                ind_loc = IND
                val_loc = VAL
                while message[ind_loc] != '|':
                    current_metric = message[ind_loc]
                    if current_metric == 'reset_log':
                        bootstrap_period_finished_this = 1
                        bootstrap_period_finished_any = 1
                        break
                    elif current_metric in metric_list:
                        current_metric_index = metric_list.index(current_metric)
                        if current_metric == 'rx_up':
                            tx_node_index = non_root_address_list.index(message[ADDR]) + 1 # index in bootstrap_period_parsed array
                            bootstrap_period_parsed[tx_node_index][current_metric_index] = message[val_loc]
                            break
                        elif current_metric == 'tx_down':
                            rx_node_index = non_root_address_list.index(message[ADDR]) + 1 # index in bootstrap_period_parsed array
                            bootstrap_period_parsed[rx_node_index][current_metric_index] = message[val_loc]
                            break
                        elif current_metric == 'lt_up_sum':
                            tx_node_index = non_root_address_list.index(message[ADDR]) + 1 # index in bootstrap_period_parsed array
                            bootstrap_period_parsed[tx_node_index][current_metric_index] = message[val_loc]
                            break
                        else:
                            bootstrap_period_parsed[ROOT_INDEX][current_metric_index] = message[val_loc]
                    ind_loc += 2
                    val_loc += 2
                    if ind_loc > len(message) or val_loc > len(message):
                            break
                if bootstrap_period_finished_this == 1:
                    break
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
        elif result_list[j] == 'uLT':
            if int(bootstrap_period_parsed[i][metric_list.index('rx_up')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('lt_up_sum')]) / float(bootstrap_period_parsed[i][metric_list.index('rx_up')]))
        elif result_list[j] == 'dLT':
            if int(bootstrap_period_parsed[i][metric_list.index('rx_down')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('lt_down_sum')]) / float(bootstrap_period_parsed[i][metric_list.index('rx_down')]))
        elif result_list[j] == 'LT':
            if int(bootstrap_period_parsed[i][metric_list.index('rx_up')]) + int(bootstrap_period_parsed[i][metric_list.index('rx_down')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round((float(bootstrap_period_parsed[i][metric_list.index('lt_up_sum')]) + float(bootstrap_period_parsed[i][metric_list.index('lt_down_sum')])) / (float(bootstrap_period_parsed[i][metric_list.index('rx_up')]) + float(bootstrap_period_parsed[i][metric_list.index('rx_down')])))
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
            tot_enqueue = float(bootstrap_period_parsed[i][metric_list.index('ip_enq')])# + float(bootstrap_period_parsed[i][metric_list.index('ka_enq')]) + float(bootstrap_period_parsed[i][metric_list.index('eb_enq')])
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
        elif result_list[j] == 'IUQL':
            tot_qloss = int(bootstrap_period_parsed[i][metric_list.index('ip_udp_qloss')])# + int(bootstrap_period_parsed[i][metric_list.index('ka_qloss')]) + int(bootstrap_period_parsed[i][metric_list.index('eb_qloss')])
            bootstrap_period_result[i][j] = tot_qloss
        elif result_list[j] == 'IUQR':
            tot_qloss = float(bootstrap_period_parsed[i][metric_list.index('ip_udp_qloss')])# + float(bootstrap_period_parsed[i][metric_list.index('ka_qloss')]) + float(bootstrap_period_parsed[i][metric_list.index('eb_qloss')])
            tot_enqueue = float(bootstrap_period_parsed[i][metric_list.index('ip_udp_enq')])# + float(bootstrap_period_parsed[i][metric_list.index('ka_enq')]) + float(bootstrap_period_parsed[i][metric_list.index('eb_enq')])
            if tot_qloss + tot_enqueue == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(tot_qloss) / (float(tot_qloss) + float(tot_enqueue)) * 100, 2)
        elif result_list[j] == 'IULL':
            tot_uc_noack = int(bootstrap_period_parsed[i][metric_list.index('ip_uc_udp_noack')])# + int(bootstrap_period_parsed[i][metric_list.index('ka_noack')])
            bootstrap_period_result[i][j] = tot_uc_noack
        elif result_list[j] == 'IULR':
            tot_uc_noack = float(bootstrap_period_parsed[i][metric_list.index('ip_uc_udp_noack')])# + float(bootstrap_period_parsed[i][metric_list.index('ka_noack')])
            tot_uc_ok = float(bootstrap_period_parsed[i][metric_list.index('ip_uc_udp_ok')])# + float(bootstrap_period_parsed[i][metric_list.index('ka_ok')])
            if tot_uc_noack + tot_uc_ok == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(tot_uc_noack) / (float(tot_uc_noack) + float(tot_uc_ok)) * 100, 2)
        elif result_list[j] == 'InQL':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('input_full')]
        elif result_list[j] == 'linkE':
            tot_uc_tx = float(bootstrap_period_parsed[i][metric_list.index('ka_tx')]) + float(bootstrap_period_parsed[i][metric_list.index('ip_uc_tx')])
            tot_uc_ok = float(bootstrap_period_parsed[i][metric_list.index('ka_ok')]) + float(bootstrap_period_parsed[i][metric_list.index('ip_uc_ok')])
            if tot_uc_ok == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(tot_uc_tx) / float(tot_uc_ok), 2)
        elif result_list[j] == 'leave':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('leaving')]
        elif result_list[j] == 'dc':
            if float(bootstrap_period_parsed[i][metric_list.index('dc_count')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('dc_total_sum')]) / float(bootstrap_period_parsed[i][metric_list.index('dc_count')]) / 100, 2)
        elif result_list[j] == 'e_drop':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('e_drop')]
        elif result_list[j] == 'SCR':
            if float(bootstrap_period_parsed[i][metric_list.index('asso_ts')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                all_scheduled = float(bootstrap_period_parsed[i][metric_list.index('sch_eb')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('sch_bc')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('sch_uc')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('sch_pp_tx')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('sch_pp_rx')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('sch_odp_tx')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('sch_odp_rx')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('sch_bc_bst_tx')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('sch_bc_bst_rx')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('sch_uc_bst_tx')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('sch_uc_bst_rx')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('sch_pp_bst_tx')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('sch_pp_bst_rx')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('bc_ep_tx_ts')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('bc_ep_rx_ts')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('uc_ep_tx_ts')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('uc_ep_rx_ts')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('pp_ep_tx_ts')]) + \
                              + float(bootstrap_period_parsed[i][metric_list.index('pp_ep_rx_ts')])
                bootstrap_period_result[i][j] = round(all_scheduled / float(bootstrap_period_parsed[i][metric_list.index('asso_ts')]) * 100, 2)
        elif result_list[j] == 'CTOR':
            if float(bootstrap_period_parsed[i][metric_list.index('sch_bc')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('bc_tx_op')]) / float(bootstrap_period_parsed[i][metric_list.index('sch_bc')]) * 100, 2)
        elif result_list[j] == 'CROR':
            if float(bootstrap_period_parsed[i][metric_list.index('sch_bc')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('bc_rx_op')]) / float(bootstrap_period_parsed[i][metric_list.index('sch_bc')]) * 100, 2)
        elif result_list[j] == 'UTOR':
            if float(bootstrap_period_parsed[i][metric_list.index('sch_uc')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('uc_tx_op')]) / float(bootstrap_period_parsed[i][metric_list.index('sch_uc')]) * 100, 2)
        elif result_list[j] == 'UROR':
            if float(bootstrap_period_parsed[i][metric_list.index('sch_uc')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('uc_rx_op')]) / float(bootstrap_period_parsed[i][metric_list.index('sch_uc')]) * 100, 2)
        elif result_list[j] == 'PTOR':
            if float(bootstrap_period_parsed[i][metric_list.index('sch_pp_tx')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('pp_tx_op')]) / float(bootstrap_period_parsed[i][metric_list.index('sch_pp_tx')]) * 100, 2)
        elif result_list[j] == 'PROR':
            if float(bootstrap_period_parsed[i][metric_list.index('sch_pp_rx')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('pp_rx_op')]) / float(bootstrap_period_parsed[i][metric_list.index('sch_pp_rx')]) * 100, 2)
        elif result_list[j] == 'BTOR':
            all_data_tx_operations = float(bootstrap_period_parsed[i][metric_list.index('uc_tx_op')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('pp_tx_op')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('uc_bst_tx_op')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('pp_bst_tx_op')])
            if all_data_tx_operations == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                dbt_data_tx_operations = float(bootstrap_period_parsed[i][metric_list.index('uc_bst_tx_op')]) + \
                                        float(bootstrap_period_parsed[i][metric_list.index('pp_bst_tx_op')])
                bootstrap_period_result[i][j] = round(dbt_data_tx_operations / all_data_tx_operations * 100, 2)
        elif result_list[j] == 'BROR':
            all_data_rx_operations = float(bootstrap_period_parsed[i][metric_list.index('uc_rx_op')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('pp_rx_op')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('uc_bst_rx_op')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('pp_bst_rx_op')])
            if all_data_rx_operations == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                dbt_data_rx_operations = float(bootstrap_period_parsed[i][metric_list.index('uc_bst_rx_op')]) + \
                                        float(bootstrap_period_parsed[i][metric_list.index('pp_bst_rx_op')])
                bootstrap_period_result[i][j] = round(dbt_data_rx_operations / all_data_rx_operations * 100, 2)
        elif result_list[j] == 'OTOR':
            all_data_tx_operations = float(bootstrap_period_parsed[i][metric_list.index('uc_tx_op')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('pp_tx_op')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('odp_tx_op')])
            if all_data_tx_operations == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                odp_data_tx_operations = float(bootstrap_period_parsed[i][metric_list.index('odp_tx_op')])
                bootstrap_period_result[i][j] = round(odp_data_tx_operations / all_data_tx_operations * 100, 2)
        elif result_list[j] == 'OROR':
            all_data_rx_operations = float(bootstrap_period_parsed[i][metric_list.index('uc_rx_op')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('pp_rx_op')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('odp_rx_op')])
            if all_data_rx_operations == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                odp_data_rx_operations = float(bootstrap_period_parsed[i][metric_list.index('odp_rx_op')])
                bootstrap_period_result[i][j] = round(odp_data_rx_operations / all_data_rx_operations * 100, 2)
        elif result_list[j] == 'ETOR':
            all_data_tx_operations = float(bootstrap_period_parsed[i][metric_list.index('uc_tx_op')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('pp_tx_op')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('uc_ep_tx_rs')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('pp_ep_tx_rs')])
            if all_data_tx_operations == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                ep_data_tx_operations = float(bootstrap_period_parsed[i][metric_list.index('uc_ep_tx_rs')]) + \
                                        float(bootstrap_period_parsed[i][metric_list.index('pp_ep_tx_rs')])
                bootstrap_period_result[i][j] = round(ep_data_tx_operations / all_data_tx_operations * 100, 2)
        elif result_list[j] == 'EROR':
            all_data_rx_operations = float(bootstrap_period_parsed[i][metric_list.index('uc_rx_op')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('pp_rx_op')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('uc_ep_rx_rs')]) + \
                                     float(bootstrap_period_parsed[i][metric_list.index('pp_ep_rx_rs')])
            if all_data_rx_operations == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                ep_data_rx_operations = float(bootstrap_period_parsed[i][metric_list.index('uc_ep_rx_rs')]) + \
                                        float(bootstrap_period_parsed[i][metric_list.index('pp_ep_rx_rs')])
                bootstrap_period_result[i][j] = round(ep_data_rx_operations / all_data_rx_operations * 100, 2)
        elif result_list[j] == 'CETR':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('bc_ep_tx_rs')]
        elif result_list[j] == 'CERR':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('bc_ep_rx_rs')]
        elif result_list[j] == 'UETR':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('uc_ep_tx_rs')]
        elif result_list[j] == 'UERR':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('uc_ep_rx_rs')]
        elif result_list[j] == 'PETR':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('pp_ep_tx_rs')]
        elif result_list[j] == 'PERR':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('pp_ep_rx_rs')]
        elif result_list[j] == 'CETO':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('bc_ep_tx_ok')]
        elif result_list[j] == 'CERO':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('bc_ep_rx_ok')]
        elif result_list[j] == 'UETO':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('uc_ep_tx_ok')]
        elif result_list[j] == 'UERO':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('uc_ep_rx_ok')]
        elif result_list[j] == 'PETO':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('pp_ep_tx_ok')]
        elif result_list[j] == 'PERO':
            bootstrap_period_result[i][j] = bootstrap_period_parsed[i][metric_list.index('pp_ep_rx_ok')]
        elif result_list[j] == 'CETE':
            if float(bootstrap_period_parsed[i][metric_list.index('bc_ep_tx_ts')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('bc_ep_tx_ok')]) / float(bootstrap_period_parsed[i][metric_list.index('bc_ep_tx_ts')]), 2)
        elif result_list[j] == 'CERE':
            if float(bootstrap_period_parsed[i][metric_list.index('bc_ep_rx_ts')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('bc_ep_rx_ok')]) / float(bootstrap_period_parsed[i][metric_list.index('bc_ep_rx_ts')]), 2)
        elif result_list[j] == 'UETE':
            if float(bootstrap_period_parsed[i][metric_list.index('uc_ep_tx_ts')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('uc_ep_tx_ok')]) / float(bootstrap_period_parsed[i][metric_list.index('uc_ep_tx_ts')]), 2)
        elif result_list[j] == 'UERE':
            if float(bootstrap_period_parsed[i][metric_list.index('uc_ep_rx_ts')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('uc_ep_rx_ok')]) / float(bootstrap_period_parsed[i][metric_list.index('uc_ep_rx_ts')]), 2)
        elif result_list[j] == 'PETE':
            if float(bootstrap_period_parsed[i][metric_list.index('pp_ep_tx_ts')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('pp_ep_tx_ok')]) / float(bootstrap_period_parsed[i][metric_list.index('pp_ep_tx_ts')]), 2)
        elif result_list[j] == 'PERE':
            if float(bootstrap_period_parsed[i][metric_list.index('pp_ep_rx_ts')]) == 0:
                bootstrap_period_result[i][j] = 'NaN'
            else:
                bootstrap_period_result[i][j] = round(float(bootstrap_period_parsed[i][metric_list.index('pp_ep_rx_ok')]) / float(bootstrap_period_parsed[i][metric_list.index('pp_ep_rx_ts')]), 2)


# STEP-3-5: [bootstrap period] print bootstrap_period_result
print('----- bootstrap period -----')
for i in range(result_list.index('SCR') - 1):#RESULT_NUM - 1):
    print(result_list[i], '\t', end='')
print(result_list[result_list.index('SCR') - 1])#-1])
for i in range(NODE_NUM):
    for j in range(result_list.index('SCR') - 1):#RESULT_NUM - 1):
        print(bootstrap_period_result[i][j], '\t', end='')
    print(bootstrap_period_result[i][result_list.index('SCR') - 1])#-1])
print()

if show_all == '1':
    for i in range(result_list.index('SCR'), RESULT_NUM - 1): 
        print(result_list[i], '\t', end='')
    print(result_list[-1], '\t', end='')
    print(result_list[0])
    for i in range(NODE_NUM):
        for j in range(result_list.index('SCR'), RESULT_NUM - 1): 
            print(bootstrap_period_result[i][j], '\t', end='')
        print(bootstrap_period_result[i][-1], '\t', end='')
        print(bootstrap_period_result[i][0])
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

    file_name = 'log-' + any_iter + '-' + str(node_id) + '.txt'
    f = open(file_name, 'r', errors='ignore')
    bootstrap_period_finished_this = 0

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
                        line = f.readline()
                        continue
                    if in_data_period == 1:
                        ind_loc = IND
                        val_loc = VAL
                        while message[ind_loc] != '|':
                            current_metric = message[ind_loc]
                            if current_metric in metric_list:
                                current_metric_index = metric_list.index(current_metric)
                                if current_metric == 'lastP': # to find node_id of last parent 
                                    if message[val_loc] == '0':
                                        data_period_parsed[node_index][current_metric_index] = 0
                                    else:
                                        if message[val_loc] == ROOT_ADDR:
                                            data_period_parsed[node_index][current_metric_index] = ROOT_ID
                                        else:
                                            data_period_parsed[node_index][current_metric_index] = non_root_id_list[non_root_address_list.index(message[val_loc])]
                                else:
                                    data_period_parsed[node_index][current_metric_index] = message[val_loc]
                            ind_loc += 2
                            val_loc += 2
                            if ind_loc > len(message) or val_loc > len(message):
                                break
        line = f.readline()
    f.close()

# STEP-4-4: [data period] parse root node
ROOT_INDEX = 0
data_period_parsed[ROOT_INDEX][metric_list.index('id')] = ROOT_ID
data_period_parsed[ROOT_INDEX][metric_list.index('addr')] = ROOT_ADDR

file_name = 'log-' + any_iter + '-' + str(ROOT_ID) + '.txt'
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
                    line = f.readline()
                    continue
                if in_data_period == 1:
                    ind_loc = IND
                    val_loc = VAL
                    while message[ind_loc] != '|':
                        current_metric = message[ind_loc]
                        if current_metric in metric_list:
                            current_metric_index = metric_list.index(current_metric)
                            if current_metric == 'rx_up':
                                tx_node_index = non_root_address_list.index(message[ADDR]) + 1 # index in data_period_parsed array
                                data_period_parsed[tx_node_index][current_metric_index] = message[val_loc]
                                break
                            elif current_metric == 'tx_down':
                                rx_node_index = non_root_address_list.index(message[ADDR]) + 1 # index in data_period_parsed array
                                data_period_parsed[rx_node_index][current_metric_index] = message[val_loc]
                                break
                            elif current_metric == 'lt_up_sum':
                                tx_node_index = non_root_address_list.index(message[ADDR]) + 1 # index in data_period_parsed array
                                data_period_parsed[tx_node_index][current_metric_index] = message[val_loc]
                                break
                            else:
                                data_period_parsed[ROOT_INDEX][current_metric_index] = message[val_loc]
                        ind_loc += 2
                        val_loc += 2
                        if ind_loc > len(message) or val_loc > len(message):
                            break
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
        elif result_list[j] == 'uLT':
            if int(data_period_parsed[i][metric_list.index('rx_up')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('lt_up_sum')]) / float(data_period_parsed[i][metric_list.index('rx_up')]))
        elif result_list[j] == 'dLT':
            if int(data_period_parsed[i][metric_list.index('rx_down')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('lt_down_sum')]) / float(data_period_parsed[i][metric_list.index('rx_down')]))
        elif result_list[j] == 'LT':
            if int(data_period_parsed[i][metric_list.index('rx_up')]) + int(data_period_parsed[i][metric_list.index('rx_down')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round((float(data_period_parsed[i][metric_list.index('lt_up_sum')]) + float(data_period_parsed[i][metric_list.index('lt_down_sum')])) / (float(data_period_parsed[i][metric_list.index('rx_up')]) + float(data_period_parsed[i][metric_list.index('rx_down')])))
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
            tot_enqueue = float(data_period_parsed[i][metric_list.index('ip_enq')])# + float(data_period_parsed[i][metric_list.index('ka_enq')]) + float(data_period_parsed[i][metric_list.index('eb_enq')])
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
        elif result_list[j] == 'IUQL':
            tot_qloss = int(data_period_parsed[i][metric_list.index('ip_udp_qloss')])# + int(data_period_parsed[i][metric_list.index('ka_qloss')]) + int(data_period_parsed[i][metric_list.index('eb_qloss')])
            data_period_result[i][j] = tot_qloss
        elif result_list[j] == 'IUQR':
            tot_qloss = float(data_period_parsed[i][metric_list.index('ip_udp_qloss')])# + float(data_period_parsed[i][metric_list.index('ka_qloss')]) + float(data_period_parsed[i][metric_list.index('eb_qloss')])
            tot_enqueue = float(data_period_parsed[i][metric_list.index('ip_udp_enq')])# + float(data_period_parsed[i][metric_list.index('ka_enq')]) + float(data_period_parsed[i][metric_list.index('eb_enq')])
            if tot_qloss + tot_enqueue == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(tot_qloss) / (float(tot_qloss) + float(tot_enqueue)) * 100, 2)
        elif result_list[j] == 'IULL':
            tot_uc_noack = int(data_period_parsed[i][metric_list.index('ip_uc_udp_noack')])# + int(data_period_parsed[i][metric_list.index('ka_noack')])
            data_period_result[i][j] = tot_uc_noack
        elif result_list[j] == 'IULR':
            tot_uc_noack = float(data_period_parsed[i][metric_list.index('ip_uc_udp_noack')])# + float(data_period_parsed[i][metric_list.index('ka_noack')])
            tot_uc_ok = float(data_period_parsed[i][metric_list.index('ip_uc_udp_ok')])# + float(data_period_parsed[i][metric_list.index('ka_ok')])
            if tot_uc_noack + tot_uc_ok == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(tot_uc_noack) / (float(tot_uc_noack) + float(tot_uc_ok)) * 100, 2)
        elif result_list[j] == 'InQL':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('input_full')]
        elif result_list[j] == 'linkE':
            tot_uc_tx = float(data_period_parsed[i][metric_list.index('ka_tx')]) + float(data_period_parsed[i][metric_list.index('ip_uc_tx')])
            tot_uc_ok = float(data_period_parsed[i][metric_list.index('ka_ok')]) + float(data_period_parsed[i][metric_list.index('ip_uc_ok')])
            if tot_uc_ok == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(tot_uc_tx) / float(tot_uc_ok), 2)
        elif result_list[j] == 'leave':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('leaving')]
        elif result_list[j] == 'dc':
            if float(data_period_parsed[i][metric_list.index('dc_count')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('dc_total_sum')]) / float(data_period_parsed[i][metric_list.index('dc_count')]) / 100, 2)
        elif result_list[j] == 'e_drop':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('e_drop')]
        elif result_list[j] == 'SCR':
            if float(data_period_parsed[i][metric_list.index('asso_ts')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                all_scheduled = float(data_period_parsed[i][metric_list.index('sch_eb')]) + \
                              + float(data_period_parsed[i][metric_list.index('sch_bc')]) + \
                              + float(data_period_parsed[i][metric_list.index('sch_uc')]) + \
                              + float(data_period_parsed[i][metric_list.index('sch_pp_tx')]) + \
                              + float(data_period_parsed[i][metric_list.index('sch_pp_rx')]) + \
                              + float(data_period_parsed[i][metric_list.index('sch_odp_tx')]) + \
                              + float(data_period_parsed[i][metric_list.index('sch_odp_rx')]) + \
                              + float(data_period_parsed[i][metric_list.index('sch_bc_bst_tx')]) + \
                              + float(data_period_parsed[i][metric_list.index('sch_bc_bst_rx')]) + \
                              + float(data_period_parsed[i][metric_list.index('sch_uc_bst_tx')]) + \
                              + float(data_period_parsed[i][metric_list.index('sch_uc_bst_rx')]) + \
                              + float(data_period_parsed[i][metric_list.index('sch_pp_bst_tx')]) + \
                              + float(data_period_parsed[i][metric_list.index('sch_pp_bst_rx')]) + \
                              + float(data_period_parsed[i][metric_list.index('bc_ep_tx_ts')]) + \
                              + float(data_period_parsed[i][metric_list.index('bc_ep_rx_ts')]) + \
                              + float(data_period_parsed[i][metric_list.index('uc_ep_tx_ts')]) + \
                              + float(data_period_parsed[i][metric_list.index('uc_ep_rx_ts')]) + \
                              + float(data_period_parsed[i][metric_list.index('pp_ep_tx_ts')]) + \
                              + float(data_period_parsed[i][metric_list.index('pp_ep_rx_ts')])
                data_period_result[i][j] = round(all_scheduled / float(data_period_parsed[i][metric_list.index('asso_ts')]) * 100, 2)
        elif result_list[j] == 'CTOR':
            if float(data_period_parsed[i][metric_list.index('sch_bc')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('bc_tx_op')]) / float(data_period_parsed[i][metric_list.index('sch_bc')]) * 100, 2)
        elif result_list[j] == 'CROR':
            if float(data_period_parsed[i][metric_list.index('sch_bc')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('bc_rx_op')]) / float(data_period_parsed[i][metric_list.index('sch_bc')]) * 100, 2)
        elif result_list[j] == 'UTOR':
            if float(data_period_parsed[i][metric_list.index('sch_uc')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('uc_tx_op')]) / float(data_period_parsed[i][metric_list.index('sch_uc')]) * 100, 2)
        elif result_list[j] == 'UROR':
            if float(data_period_parsed[i][metric_list.index('sch_uc')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('uc_rx_op')]) / float(data_period_parsed[i][metric_list.index('sch_uc')]) * 100, 2)
        elif result_list[j] == 'PTOR':
            if float(data_period_parsed[i][metric_list.index('sch_pp_tx')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('pp_tx_op')]) / float(data_period_parsed[i][metric_list.index('sch_pp_tx')]) * 100, 2)
        elif result_list[j] == 'PROR':
            if float(data_period_parsed[i][metric_list.index('sch_pp_rx')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('pp_rx_op')]) / float(data_period_parsed[i][metric_list.index('sch_pp_rx')]) * 100, 2)
        elif result_list[j] == 'BTOR':
            all_data_tx_operations = float(data_period_parsed[i][metric_list.index('uc_tx_op')]) + \
                                     float(data_period_parsed[i][metric_list.index('pp_tx_op')]) + \
                                     float(data_period_parsed[i][metric_list.index('uc_bst_tx_op')]) + \
                                     float(data_period_parsed[i][metric_list.index('pp_bst_tx_op')])
            if all_data_tx_operations == 0:
                data_period_result[i][j] = 'NaN'
            else:
                dbt_data_tx_operations = float(data_period_parsed[i][metric_list.index('uc_bst_tx_op')]) + \
                                        float(data_period_parsed[i][metric_list.index('pp_bst_tx_op')])
                data_period_result[i][j] = round(dbt_data_tx_operations / all_data_tx_operations * 100, 2)
        elif result_list[j] == 'BROR':
            all_data_rx_operations = float(data_period_parsed[i][metric_list.index('uc_rx_op')]) + \
                                     float(data_period_parsed[i][metric_list.index('pp_rx_op')]) + \
                                     float(data_period_parsed[i][metric_list.index('uc_bst_rx_op')]) + \
                                     float(data_period_parsed[i][metric_list.index('pp_bst_rx_op')])
            if all_data_rx_operations == 0:
                data_period_result[i][j] = 'NaN'
            else:
                dbt_data_rx_operations = float(data_period_parsed[i][metric_list.index('uc_bst_rx_op')]) + \
                                        float(data_period_parsed[i][metric_list.index('pp_bst_rx_op')])
                data_period_result[i][j] = round(dbt_data_rx_operations / all_data_rx_operations * 100, 2)
        elif result_list[j] == 'OTOR':
            all_data_tx_operations = float(data_period_parsed[i][metric_list.index('uc_tx_op')]) + \
                                     float(data_period_parsed[i][metric_list.index('pp_tx_op')]) + \
                                     float(data_period_parsed[i][metric_list.index('odp_tx_op')])
            if all_data_tx_operations == 0:
                data_period_result[i][j] = 'NaN'
            else:
                odp_data_tx_operations = float(data_period_parsed[i][metric_list.index('odp_tx_op')])
                data_period_result[i][j] = round(odp_data_tx_operations / all_data_tx_operations * 100, 2)
        elif result_list[j] == 'OROR':
            all_data_rx_operations = float(data_period_parsed[i][metric_list.index('uc_rx_op')]) + \
                                     float(data_period_parsed[i][metric_list.index('pp_rx_op')]) + \
                                     float(data_period_parsed[i][metric_list.index('odp_rx_op')])
            if all_data_rx_operations == 0:
                data_period_result[i][j] = 'NaN'
            else:
                odp_data_rx_operations = float(data_period_parsed[i][metric_list.index('odp_rx_op')])
                data_period_result[i][j] = round(odp_data_rx_operations / all_data_rx_operations * 100, 2)
        elif result_list[j] == 'ETOR':
            all_data_tx_operations = float(data_period_parsed[i][metric_list.index('uc_tx_op')]) + \
                                     float(data_period_parsed[i][metric_list.index('pp_tx_op')]) + \
                                     float(data_period_parsed[i][metric_list.index('uc_ep_tx_rs')]) + \
                                     float(data_period_parsed[i][metric_list.index('pp_ep_tx_rs')])
            if all_data_tx_operations == 0:
                data_period_result[i][j] = 'NaN'
            else:
                ep_data_tx_operations = float(data_period_parsed[i][metric_list.index('uc_ep_tx_rs')]) + \
                                        float(data_period_parsed[i][metric_list.index('pp_ep_tx_rs')])
                data_period_result[i][j] = round(ep_data_tx_operations / all_data_tx_operations * 100, 2)
        elif result_list[j] == 'EROR':
            all_data_rx_operations = float(data_period_parsed[i][metric_list.index('uc_rx_op')]) + \
                                     float(data_period_parsed[i][metric_list.index('pp_rx_op')]) + \
                                     float(data_period_parsed[i][metric_list.index('uc_ep_rx_rs')]) + \
                                     float(data_period_parsed[i][metric_list.index('pp_ep_rx_rs')])
            if all_data_rx_operations == 0:
                data_period_result[i][j] = 'NaN'
            else:
                ep_data_rx_operations = float(data_period_parsed[i][metric_list.index('uc_ep_rx_rs')]) + \
                                        float(data_period_parsed[i][metric_list.index('pp_ep_rx_rs')])
                data_period_result[i][j] = round(ep_data_rx_operations / all_data_rx_operations * 100, 2)
        elif result_list[j] == 'CETR':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('bc_ep_tx_rs')]
        elif result_list[j] == 'CERR':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('bc_ep_rx_rs')]
        elif result_list[j] == 'UETR':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('uc_ep_tx_rs')]
        elif result_list[j] == 'UERR':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('uc_ep_rx_rs')]
        elif result_list[j] == 'PETR':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('pp_ep_tx_rs')]
        elif result_list[j] == 'PERR':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('pp_ep_rx_rs')]
        elif result_list[j] == 'CETO':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('bc_ep_tx_ok')]
        elif result_list[j] == 'CERO':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('bc_ep_rx_ok')]
        elif result_list[j] == 'UETO':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('uc_ep_tx_ok')]
        elif result_list[j] == 'UERO':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('uc_ep_rx_ok')]
        elif result_list[j] == 'PETO':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('pp_ep_tx_ok')]
        elif result_list[j] == 'PERO':
            data_period_result[i][j] = data_period_parsed[i][metric_list.index('pp_ep_rx_ok')]
        elif result_list[j] == 'CETE':
            if float(data_period_parsed[i][metric_list.index('bc_ep_tx_ts')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('bc_ep_tx_ok')]) / float(data_period_parsed[i][metric_list.index('bc_ep_tx_ts')]), 2)
        elif result_list[j] == 'CERE':
            if float(data_period_parsed[i][metric_list.index('bc_ep_rx_ts')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('bc_ep_rx_ok')]) / float(data_period_parsed[i][metric_list.index('bc_ep_rx_ts')]), 2)
        elif result_list[j] == 'UETE':
            if float(data_period_parsed[i][metric_list.index('uc_ep_tx_ts')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('uc_ep_tx_ok')]) / float(data_period_parsed[i][metric_list.index('uc_ep_tx_ts')]), 2)
        elif result_list[j] == 'UERE':
            if float(data_period_parsed[i][metric_list.index('uc_ep_rx_ts')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('uc_ep_rx_ok')]) / float(data_period_parsed[i][metric_list.index('uc_ep_rx_ts')]), 2)
        elif result_list[j] == 'PETE':
            if float(data_period_parsed[i][metric_list.index('pp_ep_tx_ts')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('pp_ep_tx_ok')]) / float(data_period_parsed[i][metric_list.index('pp_ep_tx_ts')]), 2)
        elif result_list[j] == 'PERE':
            if float(data_period_parsed[i][metric_list.index('pp_ep_rx_ts')]) == 0:
                data_period_result[i][j] = 'NaN'
            else:
                data_period_result[i][j] = round(float(data_period_parsed[i][metric_list.index('pp_ep_rx_ok')]) / float(data_period_parsed[i][metric_list.index('pp_ep_rx_ts')]), 2)


# STEP-4-6: [data period] print data_period_result
print('----- data period -----')
for i in range(result_list.index('SCR') - 1):#RESULT_NUM - 1):
    print(result_list[i], '\t', end='')
print(result_list[result_list.index('SCR') - 1])#-1])
for i in range(NODE_NUM):
    for j in range(result_list.index('SCR') - 1):#RESULT_NUM - 1):
        print(data_period_result[i][j], '\t', end='')
    print(data_period_result[i][result_list.index('SCR') - 1])#-1])
print()

if show_all == '1':
    for i in range(result_list.index('SCR'), RESULT_NUM - 1): 
        print(result_list[i], '\t', end='')
    print(result_list[-1], '\t', end='')
    print(result_list[0])
    for i in range(NODE_NUM):
        for j in range(result_list.index('SCR'), RESULT_NUM - 1): 
            print(data_period_result[i][j], '\t', end='')
        print(data_period_result[i][-1], '\t', end='') 
        print(data_period_result[i][0])
    print()