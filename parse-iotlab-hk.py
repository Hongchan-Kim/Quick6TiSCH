# parse-iotlab-hk.py
import argparse


# Get evaluation information
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


# Extract the number of nodes
node_num = 0
file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + any_id + '.txt'
f = open(file_name, 'r', errors='ignore')
print(file_name)

line = f.readline()
while line:
    if len(line) > 1:
        line = line.replace('\n', '')
        line_prefix = line.split(':')[0]
        if line_prefix == '[HK-N':
            line_body = line.split('] ')[1].split(' ')
            if line_body[0] != 'end':
                node_num += 1
            else:
                break
    line = f.readline()
f.close()
NODE_NUM = node_num

print(NODE_NUM, 'nodes')
print()


# Metric keys to parse
key_list = ['reset_log', 'rs_q_except_eb', 'rs_opku',
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
            'sch_bc_upa_tx', 'sch_bc_upa_rx', 'sch_uc_upa_tx', 'sch_uc_upa_rx', 'sch_pp_upa_tx', 'sch_pp_upa_rx',  
            'eb_tx_op', 'eb_rx_op', 'bc_tx_op', 'bc_rx_op', 'uc_tx_op', 'uc_rx_op',
            'pp_tx_op', 'pp_rx_op', 'odp_tx_op', 'odp_rx_op',
            'bc_bst_tx_op', 'bc_bst_rx_op', 'uc_bst_tx_op', 'uc_bst_rx_op', 'pp_bst_tx_op', 'pp_bst_rx_op',
            'bc_upa_tx_rs', 'bc_upa_rx_rs', 'uc_upa_tx_rs', 'uc_upa_rx_rs', 'pp_upa_tx_rs', 'pp_upa_rx_rs',
            'bc_upa_tx_ok', 'bc_upa_rx_ok', 'uc_upa_tx_ok', 'uc_upa_rx_ok', 'pp_upa_tx_ok', 'pp_upa_rx_ok',
            'bc_upa_tx_ts', 'bc_upa_rx_ts', 'uc_upa_tx_ts', 'uc_upa_rx_ts', 'pp_upa_tx_ts', 'pp_upa_rx_ts',
            'ps', 'lastP', 'local_repair',
            'dis_send', 'dioU_send', 'dioM_send',
            'daoP_send', 'daoN_send', 'daoP_fwd', 'daoN_fwd', 'daoA_send',
            'hopD_now', 'hopD_sum', 'hopD_cnt', 'rdt',
            'subtree_now', 'subtree_sum', 'subtree_cnt',
            'dc_count', 'dc_tx_sum', 'dc_rx_sum', 'dc_total_sum']
KEY_NUM = len(key_list)


# List to store parsed result
bootstrap_period_parsed = [[0 for i in range(KEY_NUM)] for j in range(NODE_NUM)]
data_period_parsed = [[0 for i in range(KEY_NUM)] for j in range(NODE_NUM)]


# Parse root node first
ROOT_ID = 1
ROOT_INDEX = 0
root_node_bootstrap_finished = 0

file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + str(ROOT_ID) + '.txt'
f = open(file_name, 'r', errors='ignore')
print(file_name)

target_parsed_list = bootstrap_period_parsed

line = f.readline()
while line:
    if len(line) > 1:
        line = line.replace('\n', '')
        line_prefix = line.split(':')[0]
        if line_prefix == '[HK-P':
            line_body = line.split('] ')[1].split(' ')
            key_loc = 0
            val_loc = 1
            while line_body[key_loc] != '|':
                curr_key = line_body[key_loc]
                curr_val = line_body[val_loc]
                if curr_key == 'reset_log':
                    data_period_parsed[ROOT_INDEX][key_list.index('lastP')] \
                        = bootstrap_period_parsed[ROOT_INDEX][key_list.index('lastP')]
                    target_parsed_list = data_period_parsed
                    root_node_bootstrap_finished = 1
                if curr_key in key_list:
                    curr_key_index = key_list.index(curr_key)
                    if curr_key == 'rx_up' or curr_key == 'lt_up_sum':
                        tx_node_id = int(line_body[line_body.index('from') + 1])
                        tx_node_index = tx_node_id - 1
                        target_parsed_list[tx_node_index][curr_key_index] = int(curr_val)
                    elif curr_key == 'tx_down':
                        rx_node_id = int(line_body[line_body.index('to') + 1])
                        rx_node_index = rx_node_id - 1
                        target_parsed_list[rx_node_index][curr_key_index] = int(curr_val)
                    else:
                        target_parsed_list[ROOT_INDEX][curr_key_index] = int(curr_val)
                key_loc += 2
                val_loc += 2
                if key_loc >= len(line_body) or val_loc >= len(line_body):
                    break
    line = f.readline()
f.close()


# Parse non-root nodes next
for node_index in range(1, NODE_NUM):
    node_id = node_index + 1
    file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + str(node_id) + '.txt'
    f = open(file_name, 'r', errors='ignore')
    print(file_name)

    target_parsed_list = bootstrap_period_parsed

    line = f.readline()
    while line:
        if len(line) > 1:
            line = line.replace('\n', '')
            line_prefix = line.split(':')[0]
            if line_prefix == '[HK-P':
                line_body = line.split('] ')[1].split(' ')
                key_loc = 0
                val_loc = 1
                while line_body[key_loc] != '|':
                    curr_key = line_body[key_loc]
                    curr_val = line_body[val_loc]
                    if curr_key == 'reset_log':
                        data_period_parsed[node_index][key_list.index('lastP')] \
                            = bootstrap_period_parsed[node_index][key_list.index('lastP')]
                        target_parsed_list = data_period_parsed
                    if curr_key in key_list:
                        curr_key_index = key_list.index(curr_key)
                        target_parsed_list[node_index][curr_key_index] = int(curr_val)
                    key_loc += 2
                    val_loc += 2
                    if key_loc >= len(line_body) or val_loc >= len(line_body):
                        break
        line = f.readline()
    f.close()


# Derive result from parsed data
result_list = ['id', 'boot',
            'tx_up', 'rx_up', 'uPdr', 'tx_dw', 'rx_dw', 'dPdr', 'pdr', 'uLT', 'dLT', 'LT',
            'lastP', 'ps', 'hopD', 'aHopD', 'STN', 'aSTN', 
            'IPQL', 'IPQR', 'IPLL', 'IPLR', 'IUQL', 'IUQR', 'IULL', 'IULR', 'InQL', 'linkE', 
            'leave', 'dc',
            'UTS', 'UTR', 'UTO', 'UTU', 'URS', 'URR', 'URO', 'URU', 'UU',
            'SCR', 'CTOR', 'CROR', 'UTOR', 'UROR', 'PTOR', 'PROR',
            'BTOR', 'BROR', 'OTOR', 'OROR', 'ETOR', 'EROR', 
            'CETR', 'CERR', 'UETR', 'UERR', 'PETR', 'PERR',
            'CETO', 'CERO', 'UETO', 'UERO', 'PETO', 'PERO',
            'CETE', 'CERE', 'UETE', 'UERE', 'PETE', 'PERE'
            ]
RESULT_NUM = len(result_list)

bootstrap_period_result = [[0 for i in range(RESULT_NUM)] for j in range(NODE_NUM)]
data_period_result = [[0 for i in range(RESULT_NUM)] for j in range(NODE_NUM)]

for (target_parsed_list, target_result_list) in [(bootstrap_period_parsed, bootstrap_period_result), (data_period_parsed, data_period_result)]:
    for i in range(NODE_NUM):
        for j in range(RESULT_NUM):
            if result_list[j] == 'id':
                target_result_list[i][j] = str(i + 1)
            if result_list[j] == 'boot':
                if target_parsed_list[i][key_list.index('reset_log')] == 1 and \
                    target_parsed_list[i][key_list.index('rs_q_except_eb')] == 0 and \
                    target_parsed_list[i][key_list.index('rs_opku')] == 1:
                    target_result_list[i][j] = '1'
                else:
                    target_result_list[i][j] = '0'
            elif result_list[j] == 'tx_up':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('tx_up')]
            elif result_list[j] == 'rx_up':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('rx_up')]
            elif result_list[j] == 'uPdr':
                if i == ROOT_INDEX or int(target_parsed_list[i][key_list.index('tx_up')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('rx_up')]) / float(target_parsed_list[i][key_list.index('tx_up')]) * 100, 2)
            elif result_list[j] == 'tx_dw':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('tx_down')]
            elif result_list[j] == 'rx_dw':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('rx_down')]
            elif result_list[j] == 'dPdr':
                if i == ROOT_INDEX or int(target_parsed_list[i][key_list.index('tx_down')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('rx_down')]) / float(target_parsed_list[i][key_list.index('tx_down')]) * 100, 2)
            elif result_list[j] == 'pdr':
                if i == ROOT_INDEX or int(target_parsed_list[i][key_list.index('tx_up')]) + int(target_parsed_list[i][key_list.index('tx_down')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round((float(target_parsed_list[i][key_list.index('rx_up')]) + float(target_parsed_list[i][key_list.index('rx_down')])) / (float(target_parsed_list[i][key_list.index('tx_up')]) + float(target_parsed_list[i][key_list.index('tx_down')])) * 100, 2)
            elif result_list[j] == 'uLT':
                if int(target_parsed_list[i][key_list.index('rx_up')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('lt_up_sum')]) / float(target_parsed_list[i][key_list.index('rx_up')]))
            elif result_list[j] == 'dLT':
                if int(target_parsed_list[i][key_list.index('rx_down')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('lt_down_sum')]) / float(target_parsed_list[i][key_list.index('rx_down')]))
            elif result_list[j] == 'LT':
                if int(target_parsed_list[i][key_list.index('rx_up')]) + int(target_parsed_list[i][key_list.index('rx_down')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round((float(target_parsed_list[i][key_list.index('lt_up_sum')]) + float(target_parsed_list[i][key_list.index('lt_down_sum')])) / (float(target_parsed_list[i][key_list.index('rx_up')]) + float(target_parsed_list[i][key_list.index('rx_down')])))
            elif result_list[j] == 'lastP':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('lastP')]
            elif result_list[j] == 'ps':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('ps')]
            elif result_list[j] == 'hopD':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('hopD_now')]
            elif result_list[j] == 'aHopD':
                if int(target_parsed_list[i][key_list.index('hopD_cnt')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('hopD_sum')]) / float(target_parsed_list[i][key_list.index('hopD_cnt')]), 2)
            elif result_list[j] == 'STN':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('subtree_now')]
            elif result_list[j] == 'aSTN':
                if int(target_parsed_list[i][key_list.index('subtree_cnt')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('subtree_sum')]) / float(target_parsed_list[i][key_list.index('subtree_cnt')]), 2)
            elif result_list[j] == 'IPQL':
                tot_qloss = int(target_parsed_list[i][key_list.index('ip_qloss')])# + int(target_parsed_list[i][key_list.index('ka_qloss')]) + int(target_parsed_list[i][key_list.index('eb_qloss')])
                target_result_list[i][j] = tot_qloss
            elif result_list[j] == 'IPQR':
                tot_qloss = float(target_parsed_list[i][key_list.index('ip_qloss')])# + float(target_parsed_list[i][key_list.index('ka_qloss')]) + float(target_parsed_list[i][key_list.index('eb_qloss')])
                tot_enqueue = float(target_parsed_list[i][key_list.index('ip_enq')])# + float(target_parsed_list[i][key_list.index('ka_enq')]) + float(target_parsed_list[i][key_list.index('eb_enq')])
                if tot_qloss + tot_enqueue == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(tot_qloss) / (float(tot_qloss) + float(tot_enqueue)) * 100, 2)
            elif result_list[j] == 'IPLL':
                tot_uc_noack = int(target_parsed_list[i][key_list.index('ip_uc_noack')])# + int(target_parsed_list[i][key_list.index('ka_noack')])
                target_result_list[i][j] = tot_uc_noack
            elif result_list[j] == 'IPLR':
                tot_uc_noack = float(target_parsed_list[i][key_list.index('ip_uc_noack')])# + float(target_parsed_list[i][key_list.index('ka_noack')])
                tot_uc_ok = float(target_parsed_list[i][key_list.index('ip_uc_ok')])# + float(target_parsed_list[i][key_list.index('ka_ok')])
                if tot_uc_noack + tot_uc_ok == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(tot_uc_noack) / (float(tot_uc_noack) + float(tot_uc_ok)) * 100, 2)
            elif result_list[j] == 'IUQL':
                tot_qloss = int(target_parsed_list[i][key_list.index('ip_udp_qloss')])# + int(target_parsed_list[i][key_list.index('ka_qloss')]) + int(target_parsed_list[i][key_list.index('eb_qloss')])
                target_result_list[i][j] = tot_qloss
            elif result_list[j] == 'IUQR':
                tot_qloss = float(target_parsed_list[i][key_list.index('ip_udp_qloss')])# + float(target_parsed_list[i][key_list.index('ka_qloss')]) + float(target_parsed_list[i][key_list.index('eb_qloss')])
                tot_enqueue = float(target_parsed_list[i][key_list.index('ip_udp_enq')])# + float(target_parsed_list[i][key_list.index('ka_enq')]) + float(target_parsed_list[i][key_list.index('eb_enq')])
                if tot_qloss + tot_enqueue == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(tot_qloss) / (float(tot_qloss) + float(tot_enqueue)) * 100, 2)
            elif result_list[j] == 'IULL':
                tot_uc_noack = int(target_parsed_list[i][key_list.index('ip_uc_udp_noack')])# + int(target_parsed_list[i][key_list.index('ka_noack')])
                target_result_list[i][j] = tot_uc_noack
            elif result_list[j] == 'IULR':
                tot_uc_noack = float(target_parsed_list[i][key_list.index('ip_uc_udp_noack')])# + float(target_parsed_list[i][key_list.index('ka_noack')])
                tot_uc_ok = float(target_parsed_list[i][key_list.index('ip_uc_udp_ok')])# + float(target_parsed_list[i][key_list.index('ka_ok')])
                if tot_uc_noack + tot_uc_ok == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(tot_uc_noack) / (float(tot_uc_noack) + float(tot_uc_ok)) * 100, 2)
            elif result_list[j] == 'InQL':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('input_full')]
            elif result_list[j] == 'linkE':
                tot_uc_tx = float(target_parsed_list[i][key_list.index('ka_tx')]) + float(target_parsed_list[i][key_list.index('ip_uc_tx')])
                tot_uc_ok = float(target_parsed_list[i][key_list.index('ka_ok')]) + float(target_parsed_list[i][key_list.index('ip_uc_ok')])
                if tot_uc_ok == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(tot_uc_tx) / float(tot_uc_ok), 2)
            elif result_list[j] == 'leave':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('leaving')]
            elif result_list[j] == 'dc':
                if float(target_parsed_list[i][key_list.index('dc_count')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('dc_total_sum')]) / float(target_parsed_list[i][key_list.index('dc_count')]) / 100, 2)
            elif result_list[j] == 'e_drop':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('e_drop')]

            elif result_list[j] == 'UTS':
                target_result_list[i][j] = int(target_parsed_list[i][key_list.index('sch_bc_upa_tx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_uc_upa_tx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_pp_upa_tx')]) + \
                                                int(target_parsed_list[i][key_list.index('bc_upa_tx_ts')]) + \
                                                int(target_parsed_list[i][key_list.index('uc_upa_tx_ts')]) + \
                                                int(target_parsed_list[i][key_list.index('pp_upa_tx_ts')])
            elif result_list[j] == 'UTR':
                target_result_list[i][j] = int(target_parsed_list[i][key_list.index('sch_bc_upa_tx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_uc_upa_tx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_pp_upa_tx')]) + \
                                                int(target_parsed_list[i][key_list.index('bc_upa_tx_rs')]) + \
                                                int(target_parsed_list[i][key_list.index('uc_upa_tx_rs')]) + \
                                                int(target_parsed_list[i][key_list.index('pp_upa_tx_rs')])
            elif result_list[j] == 'UTO':
                target_result_list[i][j] = int(target_parsed_list[i][key_list.index('sch_bc_upa_tx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_uc_upa_tx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_pp_upa_tx')]) + \
                                                int(target_parsed_list[i][key_list.index('bc_upa_tx_ok')]) + \
                                                int(target_parsed_list[i][key_list.index('uc_upa_tx_ok')]) + \
                                                int(target_parsed_list[i][key_list.index('pp_upa_tx_ok')])
            elif result_list[j] == 'UTU':
                if float(target_result_list[i][result_list.index('UTS')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_result_list[i][result_list.index('UTO')]) / float(target_result_list[i][result_list.index('UTS')]), 2)
            elif result_list[j] == 'URS':
                target_result_list[i][j] = int(target_parsed_list[i][key_list.index('sch_bc_upa_rx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_uc_upa_rx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_pp_upa_rx')]) + \
                                                int(target_parsed_list[i][key_list.index('bc_upa_rx_ts')]) + \
                                                int(target_parsed_list[i][key_list.index('uc_upa_rx_ts')]) + \
                                                int(target_parsed_list[i][key_list.index('pp_upa_rx_ts')])
            elif result_list[j] == 'URR':
                target_result_list[i][j] = int(target_parsed_list[i][key_list.index('sch_bc_upa_rx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_uc_upa_rx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_pp_upa_rx')]) + \
                                                int(target_parsed_list[i][key_list.index('bc_upa_rx_rs')]) + \
                                                int(target_parsed_list[i][key_list.index('uc_upa_rx_rs')]) + \
                                                int(target_parsed_list[i][key_list.index('pp_upa_rx_rs')])
            elif result_list[j] == 'URO':
                target_result_list[i][j] = int(target_parsed_list[i][key_list.index('sch_bc_upa_rx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_uc_upa_rx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_pp_upa_rx')]) + \
                                                int(target_parsed_list[i][key_list.index('bc_upa_rx_ok')]) + \
                                                int(target_parsed_list[i][key_list.index('uc_upa_rx_ok')]) + \
                                                int(target_parsed_list[i][key_list.index('pp_upa_rx_ok')])
            elif result_list[j] == 'URU':
                if float(target_result_list[i][result_list.index('URS')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_result_list[i][result_list.index('URO')]) / float(target_result_list[i][result_list.index('URS')]), 2)
            elif result_list[j] == 'UU':
                if float(target_result_list[i][result_list.index('UTS')]) + float(target_result_list[i][result_list.index('URS')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round((float(target_result_list[i][result_list.index('UTO')]) + float(target_result_list[i][result_list.index('URO')])) / (float(target_result_list[i][result_list.index('UTS')]) + float(target_result_list[i][result_list.index('URS')])), 2)

            elif result_list[j] == 'SCR':
                if float(target_parsed_list[i][key_list.index('asso_ts')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    all_scheduled = float(target_parsed_list[i][key_list.index('sch_eb')]) + \
                                + float(target_parsed_list[i][key_list.index('sch_bc')]) + \
                                + float(target_parsed_list[i][key_list.index('sch_uc')]) + \
                                + float(target_parsed_list[i][key_list.index('sch_pp_tx')]) + \
                                + float(target_parsed_list[i][key_list.index('sch_pp_rx')]) + \
                                + float(target_parsed_list[i][key_list.index('sch_odp_tx')]) + \
                                + float(target_parsed_list[i][key_list.index('sch_odp_rx')]) + \
                                + float(target_parsed_list[i][key_list.index('sch_bc_bst_tx')]) + \
                                + float(target_parsed_list[i][key_list.index('sch_bc_bst_rx')]) + \
                                + float(target_parsed_list[i][key_list.index('sch_uc_bst_tx')]) + \
                                + float(target_parsed_list[i][key_list.index('sch_uc_bst_rx')]) + \
                                + float(target_parsed_list[i][key_list.index('sch_pp_bst_tx')]) + \
                                + float(target_parsed_list[i][key_list.index('sch_pp_bst_rx')]) + \
                                + float(target_parsed_list[i][key_list.index('bc_upa_tx_ts')]) + \
                                + float(target_parsed_list[i][key_list.index('bc_upa_rx_ts')]) + \
                                + float(target_parsed_list[i][key_list.index('uc_upa_tx_ts')]) + \
                                + float(target_parsed_list[i][key_list.index('uc_upa_rx_ts')]) + \
                                + float(target_parsed_list[i][key_list.index('pp_upa_tx_ts')]) + \
                                + float(target_parsed_list[i][key_list.index('pp_upa_rx_ts')])
                    target_result_list[i][j] = round(all_scheduled / float(target_parsed_list[i][key_list.index('asso_ts')]) * 100, 2)
            elif result_list[j] == 'CTOR':
                if float(target_parsed_list[i][key_list.index('sch_bc')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('bc_tx_op')]) / float(target_parsed_list[i][key_list.index('sch_bc')]) * 100, 2)
            elif result_list[j] == 'CROR':
                if float(target_parsed_list[i][key_list.index('sch_bc')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('bc_rx_op')]) / float(target_parsed_list[i][key_list.index('sch_bc')]) * 100, 2)
            elif result_list[j] == 'UTOR':
                if float(target_parsed_list[i][key_list.index('sch_uc')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('uc_tx_op')]) / float(target_parsed_list[i][key_list.index('sch_uc')]) * 100, 2)
            elif result_list[j] == 'UROR':
                if float(target_parsed_list[i][key_list.index('sch_uc')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('uc_rx_op')]) / float(target_parsed_list[i][key_list.index('sch_uc')]) * 100, 2)
            elif result_list[j] == 'PTOR':
                if float(target_parsed_list[i][key_list.index('sch_pp_tx')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('pp_tx_op')]) / float(target_parsed_list[i][key_list.index('sch_pp_tx')]) * 100, 2)
            elif result_list[j] == 'PROR':
                if float(target_parsed_list[i][key_list.index('sch_pp_rx')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('pp_rx_op')]) / float(target_parsed_list[i][key_list.index('sch_pp_rx')]) * 100, 2)
            elif result_list[j] == 'BTOR':
                all_data_tx_operations = float(target_parsed_list[i][key_list.index('uc_tx_op')]) + \
                                        float(target_parsed_list[i][key_list.index('pp_tx_op')]) + \
                                        float(target_parsed_list[i][key_list.index('uc_bst_tx_op')]) + \
                                        float(target_parsed_list[i][key_list.index('pp_bst_tx_op')])
                if all_data_tx_operations == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    dbt_data_tx_operations = float(target_parsed_list[i][key_list.index('uc_bst_tx_op')]) + \
                                            float(target_parsed_list[i][key_list.index('pp_bst_tx_op')])
                    target_result_list[i][j] = round(dbt_data_tx_operations / all_data_tx_operations * 100, 2)
            elif result_list[j] == 'BROR':
                all_data_rx_operations = float(target_parsed_list[i][key_list.index('uc_rx_op')]) + \
                                        float(target_parsed_list[i][key_list.index('pp_rx_op')]) + \
                                        float(target_parsed_list[i][key_list.index('uc_bst_rx_op')]) + \
                                        float(target_parsed_list[i][key_list.index('pp_bst_rx_op')])
                if all_data_rx_operations == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    dbt_data_rx_operations = float(target_parsed_list[i][key_list.index('uc_bst_rx_op')]) + \
                                            float(target_parsed_list[i][key_list.index('pp_bst_rx_op')])
                    target_result_list[i][j] = round(dbt_data_rx_operations / all_data_rx_operations * 100, 2)
            elif result_list[j] == 'OTOR':
                all_data_tx_operations = float(target_parsed_list[i][key_list.index('uc_tx_op')]) + \
                                        float(target_parsed_list[i][key_list.index('pp_tx_op')]) + \
                                        float(target_parsed_list[i][key_list.index('odp_tx_op')])
                if all_data_tx_operations == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    odp_data_tx_operations = float(target_parsed_list[i][key_list.index('odp_tx_op')])
                    target_result_list[i][j] = round(odp_data_tx_operations / all_data_tx_operations * 100, 2)
            elif result_list[j] == 'OROR':
                all_data_rx_operations = float(target_parsed_list[i][key_list.index('uc_rx_op')]) + \
                                        float(target_parsed_list[i][key_list.index('pp_rx_op')]) + \
                                        float(target_parsed_list[i][key_list.index('odp_rx_op')])
                if all_data_rx_operations == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    odp_data_rx_operations = float(target_parsed_list[i][key_list.index('odp_rx_op')])
                    target_result_list[i][j] = round(odp_data_rx_operations / all_data_rx_operations * 100, 2)
            elif result_list[j] == 'ETOR':
                all_data_tx_operations = float(target_parsed_list[i][key_list.index('uc_tx_op')]) + \
                                        float(target_parsed_list[i][key_list.index('pp_tx_op')]) + \
                                        float(target_parsed_list[i][key_list.index('uc_upa_tx_rs')]) + \
                                        float(target_parsed_list[i][key_list.index('pp_upa_tx_rs')])
                if all_data_tx_operations == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    ep_data_tx_operations = float(target_parsed_list[i][key_list.index('uc_upa_tx_rs')]) + \
                                            float(target_parsed_list[i][key_list.index('pp_upa_tx_rs')])
                    target_result_list[i][j] = round(ep_data_tx_operations / all_data_tx_operations * 100, 2)
            elif result_list[j] == 'EROR':
                all_data_rx_operations = float(target_parsed_list[i][key_list.index('uc_rx_op')]) + \
                                        float(target_parsed_list[i][key_list.index('pp_rx_op')]) + \
                                        float(target_parsed_list[i][key_list.index('uc_upa_rx_rs')]) + \
                                        float(target_parsed_list[i][key_list.index('pp_upa_rx_rs')])
                if all_data_rx_operations == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    ep_data_rx_operations = float(target_parsed_list[i][key_list.index('uc_upa_rx_rs')]) + \
                                            float(target_parsed_list[i][key_list.index('pp_upa_rx_rs')])
                    target_result_list[i][j] = round(ep_data_rx_operations / all_data_rx_operations * 100, 2)
            elif result_list[j] == 'CETR':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('bc_upa_tx_rs')]
            elif result_list[j] == 'CERR':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('bc_upa_rx_rs')]
            elif result_list[j] == 'UETR':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('uc_upa_tx_rs')]
            elif result_list[j] == 'UERR':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('uc_upa_rx_rs')]
            elif result_list[j] == 'PETR':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('pp_upa_tx_rs')]
            elif result_list[j] == 'PERR':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('pp_upa_rx_rs')]
            elif result_list[j] == 'CETO':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('bc_upa_tx_ok')]
            elif result_list[j] == 'CERO':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('bc_upa_rx_ok')]
            elif result_list[j] == 'UETO':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('uc_upa_tx_ok')]
            elif result_list[j] == 'UERO':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('uc_upa_rx_ok')]
            elif result_list[j] == 'PETO':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('pp_upa_tx_ok')]
            elif result_list[j] == 'PERO':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('pp_upa_rx_ok')]
            elif result_list[j] == 'CETE':
                if float(target_parsed_list[i][key_list.index('bc_upa_tx_ts')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('bc_upa_tx_ok')]) / float(target_parsed_list[i][key_list.index('bc_upa_tx_ts')]), 2)
            elif result_list[j] == 'CERE':
                if float(target_parsed_list[i][key_list.index('bc_upa_rx_ts')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('bc_upa_rx_ok')]) / float(target_parsed_list[i][key_list.index('bc_upa_rx_ts')]), 2)
            elif result_list[j] == 'UETE':
                if float(target_parsed_list[i][key_list.index('uc_upa_tx_ts')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('uc_upa_tx_ok')]) / float(target_parsed_list[i][key_list.index('uc_upa_tx_ts')]), 2)
            elif result_list[j] == 'UERE':
                if float(target_parsed_list[i][key_list.index('uc_upa_rx_ts')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('uc_upa_rx_ok')]) / float(target_parsed_list[i][key_list.index('uc_upa_rx_ts')]), 2)
            elif result_list[j] == 'PETE':
                if float(target_parsed_list[i][key_list.index('pp_upa_tx_ts')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('pp_upa_tx_ok')]) / float(target_parsed_list[i][key_list.index('pp_upa_tx_ts')]), 2)
            elif result_list[j] == 'PERE':
                if float(target_parsed_list[i][key_list.index('pp_upa_rx_ts')]) == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(target_parsed_list[i][key_list.index('pp_upa_rx_ok')]) / float(target_parsed_list[i][key_list.index('pp_upa_rx_ts')]), 2)


# Print derived result
print()
for target_result_list in [bootstrap_period_result, data_period_result]:
    if target_result_list == bootstrap_period_result:
        print('----- bootstrap period -----')
    else:
        print('----- data period -----')

    for i in range(result_list.index('SCR') - 1):#RESULT_NUM - 1):
        print(result_list[i], '\t', end='')
    print(result_list[result_list.index('SCR') - 1])#-1])
    for i in range(NODE_NUM):
        for j in range(result_list.index('SCR') - 1):#RESULT_NUM - 1):
            print(target_result_list[i][j], '\t', end='')
        print(target_result_list[i][result_list.index('SCR') - 1])#-1])
    print()

    if show_all == '1':
        for i in range(result_list.index('SCR'), RESULT_NUM - 1): 
            print(result_list[i], '\t', end='')
        print(result_list[-1], '\t', end='')
        print(result_list[0])
        for i in range(NODE_NUM):
            for j in range(result_list.index('SCR'), RESULT_NUM - 1): 
                print(target_result_list[i][j], '\t', end='')
            print(target_result_list[i][-1], '\t', end='')
            print(target_result_list[i][0])
        print()

    if root_node_bootstrap_finished == 0:
        print('----- in bootstrap period -----')
        break