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

print('----- Parsing info -----')
print('any_scheduler: ' + any_scheduler)
print('any_iter: ' + any_iter)
print('any_id: ' + any_id)
print('show_all: ' + show_all)
print()


# Extract the number of nodes
print("Parse the number of nodes")
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
key_list = ['logging_disrupted', 'fixed_topology', 'lite_log',
            'traffic_load', 'down_traffic_load', 'app_payload_len',
            'slot_len', 'ucsf_period',
            'with_upa', 'with_sla', 'sla_k',
            'with_dbt', 'with_a3', 'a3_max_zone',
            'reset_eval', 'rs_opku', 'rs_q', 'opku',
            'tx_up', 'rx_up', 'tx_down', 'rx_down',
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
bootstrap_period_topology_opt_parsed = [[0 for i in range(KEY_NUM)] for j in range(NODE_NUM)]
bootstrap_period_traffic_opt_parsed = [[0 for i in range(KEY_NUM)] for j in range(NODE_NUM)]
data_period_parsed = [[0 for i in range(KEY_NUM)] for j in range(NODE_NUM)]


# Parse root node first
ROOT_ID = 1
ROOT_INDEX = 0
root_node_bootstrap_finished = 0

IS_SLA_ON = 0
IS_UPA_ON = 0

print("Parse root node")
file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + str(ROOT_ID) + '.txt'
f = open(file_name, 'r', errors='ignore')
print(file_name)

target_parsed_list = bootstrap_period_topology_opt_parsed

last_tx_down = 0

line = f.readline()
while line:
    if len(line) > 1:
        line = line.replace('\n', '')
        line_prefix = line.split(':')[0]
        line_postfix = line.split(' ')[-1]
        if line_prefix == '[HK-P':
            line_body = line.split('] ')[1].split(' ')
            key_loc = 0
            val_loc = 1
            while line_body[key_loc] != '|':
                curr_key = line_body[key_loc]
                curr_val = line_body[val_loc]
                if curr_key == 'reset_eval':
                    if curr_val == '0':
                        bootstrap_period_traffic_opt_parsed[ROOT_INDEX][key_list.index('lastP')] \
                            = bootstrap_period_topology_opt_parsed[ROOT_INDEX][key_list.index('lastP')]
                        bootstrap_period_traffic_opt_parsed[ROOT_INDEX][key_list.index('opku')] \
                            = bootstrap_period_topology_opt_parsed[ROOT_INDEX][key_list.index('opku')]
                        target_parsed_list = bootstrap_period_traffic_opt_parsed
                        root_node_bootstrap_finished = 1
                        last_tx_down = 0
                    elif curr_val == '1':
                        data_period_parsed[ROOT_INDEX][key_list.index('lastP')] \
                            = bootstrap_period_traffic_opt_parsed[ROOT_INDEX][key_list.index('lastP')]
                        data_period_parsed[ROOT_INDEX][key_list.index('opku')] \
                            = bootstrap_period_traffic_opt_parsed[ROOT_INDEX][key_list.index('opku')]
                        target_parsed_list = data_period_parsed
                        root_node_bootstrap_finished = 2
                        last_tx_down = 0
                if curr_key in key_list:
                    curr_key_index = key_list.index(curr_key)
                    if curr_key == 'rx_up':
                        tx_node_id = int(line_body[line_body.index('from') + 1])
                        tx_node_index = tx_node_id - 1
                        target_parsed_list[tx_node_index][curr_key_index] = int(curr_val)
                    elif curr_key == 'tx_down':
                        rx_node_id = int(line_body[line_body.index('to') + 1])
                        rx_node_index = rx_node_id - 1
                        target_parsed_list[rx_node_index][curr_key_index] = int(curr_val)
                        if int(curr_val) - last_tx_down > 1:
                            target_parsed_list[ROOT_INDEX][key_list.index('logging_disrupted')] = 1
                        last_tx_down = int(curr_val)
                    else:
                        target_parsed_list[ROOT_INDEX][curr_key_index] = int(curr_val)
                key_loc += 2
                val_loc += 2
                if key_loc >= len(line_body) or val_loc >= len(line_body):
                    break
        if IS_SLA_ON == 0 and line_prefix == '[HK-S':
            IS_SLA_ON = 1
        if IS_UPA_ON == 0 and line_postfix == 'HK-U':
            IS_UPA_ON = 1
    line = f.readline()
f.close()


# Parse non-root nodes next
print()
print("Parse non-root nodes")
for node_index in range(1, NODE_NUM):
    node_id = node_index + 1
    file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + str(node_id) + '.txt'
    f = open(file_name, 'r', errors='ignore')
    print(file_name)

    target_parsed_list = bootstrap_period_topology_opt_parsed

    last_tx_up = 0

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
                    if curr_key == 'reset_eval':
                        if curr_val == '0':
                            bootstrap_period_traffic_opt_parsed[node_index][key_list.index('lastP')] \
                                = bootstrap_period_topology_opt_parsed[node_index][key_list.index('lastP')]
                            bootstrap_period_traffic_opt_parsed[node_index][key_list.index('opku')] \
                                = bootstrap_period_topology_opt_parsed[node_index][key_list.index('opku')]
                            target_parsed_list = bootstrap_period_traffic_opt_parsed
                            last_tx_up = 0
                        elif curr_val == '1':
                            data_period_parsed[node_index][key_list.index('lastP')] \
                                = bootstrap_period_traffic_opt_parsed[node_index][key_list.index('lastP')]
                            data_period_parsed[node_index][key_list.index('opku')] \
                                = bootstrap_period_traffic_opt_parsed[node_index][key_list.index('opku')]
                            target_parsed_list = data_period_parsed
                            last_tx_up = 0
                    if curr_key in key_list:
                        curr_key_index = key_list.index(curr_key)
                        target_parsed_list[node_index][curr_key_index] = int(curr_val)
                        if curr_key == 'tx_up':
                            if int(curr_val) - last_tx_up > 1:
                                target_parsed_list[node_index][key_list.index('logging_disrupted')] = 1
                            last_tx_up = int(curr_val)
                    key_loc += 2
                    val_loc += 2
                    if key_loc >= len(line_body) or val_loc >= len(line_body):
                        break
        line = f.readline()
    f.close()


EVAL_CONFIG_FIXED_TOPOLOGY = int(bootstrap_period_topology_opt_parsed[ROOT_INDEX][key_list.index('fixed_topology')])
EVAL_CONFIG_LITE_LOG = int(bootstrap_period_topology_opt_parsed[ROOT_INDEX][key_list.index('lite_log')])
EVAL_CONFIG_TRAFFIC_LOAD = int(bootstrap_period_topology_opt_parsed[ROOT_INDEX][key_list.index('traffic_load')])
EVAL_CONFIG_DOWN_TRAFFIC_LOAD = int(bootstrap_period_topology_opt_parsed[ROOT_INDEX][key_list.index('down_traffic_load')])
EVAL_CONFIG_APP_PAYLOAD_LEN = int(bootstrap_period_topology_opt_parsed[ROOT_INDEX][key_list.index('app_payload_len')])
EVAL_CONFIG_SLOT_LEN = int(bootstrap_period_topology_opt_parsed[ROOT_INDEX][key_list.index('slot_len')])
EVAL_CONFIG_UCSF_PERIOD = int(bootstrap_period_topology_opt_parsed[ROOT_INDEX][key_list.index('ucsf_period')])
EVAL_CONFIG_WITH_UPA = int(bootstrap_period_topology_opt_parsed[ROOT_INDEX][key_list.index('with_upa')])
EVAL_CONFIG_WITH_SLA = int(bootstrap_period_topology_opt_parsed[ROOT_INDEX][key_list.index('with_sla')])
EVAL_CONFIG_SLA_K = int(bootstrap_period_topology_opt_parsed[ROOT_INDEX][key_list.index('sla_k')])
EVAL_CONFIG_WITH_DBT = int(bootstrap_period_topology_opt_parsed[ROOT_INDEX][key_list.index('with_dbt')])
EVAL_CONFIG_WITH_A3 = int(bootstrap_period_topology_opt_parsed[ROOT_INDEX][key_list.index('with_a3')])
EVAL_CONFIG_A3_MAX_ZONE = int(bootstrap_period_topology_opt_parsed[ROOT_INDEX][key_list.index('a3_max_zone')])

EVAL_CONFIG_WITH_UPA = int(IS_UPA_ON)
EVAL_CONFIG_WITH_SLA = int(IS_SLA_ON)


# Derive result from parsed data
result_list = ['id', 'bootP', 'bootQ', 'opku',
            'tx_up', 'rx_up', 'uPdr', 'tx_dw', 'rx_dw', 'dPdr', 'pdr', 'uLT', 'dLT', 'LT', 'MuLT', 'MdLT',
            'lastP', 'ps', 'aHopD', 'aSTN', 
            'IPQL', 'IPQR', 'IPLL', 'IPLR', 'IUQL', 'IUQR', 'IULL', 'IULR', 'linkE', 
            'leave', 'dc',
            'as_ts', 'sch_eb', 'sch_bc', 'sch_uc']

sla_result_list = ['ts_l']
result_list.extend(sla_result_list)

upa_result_list = ['UTC', 'UTS', 'UTR', 'UTO', 'UTU', 'URC', 'URS', 'URR', 'URO', 'URU', 'UU']
result_list.extend(upa_result_list)

show_all_result_list = ['SCR',
            'CTOR', 'CROR', 'UTOR', 'UROR', 'PTOR', 'PROR',
            'BTOR', 'BROR', 'OTOR', 'OROR', 'ETOR', 'EROR', 
            'CETR', 'CERR', 'UETR', 'UERR', 'PETR', 'PERR',
            'CETO', 'CERO', 'UETO', 'UERO', 'PETO', 'PERO',
            'CETE', 'CERE', 'UETE', 'UERE', 'PETE', 'PERE']
result_list.extend(show_all_result_list)

RESULT_NUM = len(result_list)

bootstrap_period_topology_opt_result = [[0 for i in range(RESULT_NUM)] for j in range(NODE_NUM)]
bootstrap_period_traffic_opt_result = [[0 for i in range(RESULT_NUM)] for j in range(NODE_NUM)]
data_period_result = [[0 for i in range(RESULT_NUM)] for j in range(NODE_NUM)]

for (target_parsed_list, target_result_list) in [(bootstrap_period_topology_opt_parsed, bootstrap_period_topology_opt_result), \
                                                 (bootstrap_period_traffic_opt_parsed, bootstrap_period_traffic_opt_result), \
                                                 (data_period_parsed, data_period_result)]:
    for i in range(NODE_NUM):
        for j in range(RESULT_NUM):
            if result_list[j] == 'id':
                target_result_list[i][j] = str(i + 1)
            if result_list[j] == 'bootP':
                if target_parsed_list[i][key_list.index('rs_opku')] == 1:
                    target_result_list[i][j] = '1'
                else:
                    target_result_list[i][j] = '0'
            elif result_list[j] == 'bootQ':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('rs_q')]
            elif result_list[j] == 'opku':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('opku')]
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
                tot_qloss = int(target_parsed_list[i][key_list.index('ip_qloss')])
                target_result_list[i][j] = tot_qloss
            elif result_list[j] == 'IPQR':
                tot_qloss = float(target_parsed_list[i][key_list.index('ip_qloss')])
                tot_enqueue = float(target_parsed_list[i][key_list.index('ip_enq')])
                if tot_qloss + tot_enqueue == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(tot_qloss) / (float(tot_qloss) + float(tot_enqueue)) * 100, 2)
            elif result_list[j] == 'IPLL':
                tot_uc_noack = int(target_parsed_list[i][key_list.index('ip_uc_noack')])
                target_result_list[i][j] = tot_uc_noack
            elif result_list[j] == 'IPLR':
                tot_uc_noack = float(target_parsed_list[i][key_list.index('ip_uc_noack')])
                tot_uc_ok = float(target_parsed_list[i][key_list.index('ip_uc_ok')])
                if tot_uc_noack + tot_uc_ok == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(tot_uc_noack) / (float(tot_uc_noack) + float(tot_uc_ok)) * 100, 2)
            elif result_list[j] == 'IUQL':
                tot_qloss = int(target_parsed_list[i][key_list.index('ip_udp_qloss')])
                target_result_list[i][j] = tot_qloss
            elif result_list[j] == 'IUQR':
                tot_qloss = float(target_parsed_list[i][key_list.index('ip_udp_qloss')])
                tot_enqueue = float(target_parsed_list[i][key_list.index('ip_udp_enq')])
                if tot_qloss + tot_enqueue == 0:
                    target_result_list[i][j] = 'NaN'
                else:
                    target_result_list[i][j] = round(float(tot_qloss) / (float(tot_qloss) + float(tot_enqueue)) * 100, 2)
            elif result_list[j] == 'IULL':
                tot_uc_noack = int(target_parsed_list[i][key_list.index('ip_uc_udp_noack')])
                target_result_list[i][j] = tot_uc_noack
            elif result_list[j] == 'IULR':
                tot_uc_noack = float(target_parsed_list[i][key_list.index('ip_uc_udp_noack')])
                tot_uc_ok = float(target_parsed_list[i][key_list.index('ip_uc_udp_ok')])
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

            elif result_list[j] == 'as_ts':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('asso_ts')]
            elif result_list[j] == 'sch_eb':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('sch_eb')]
            elif result_list[j] == 'sch_bc':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('sch_bc')]
            elif result_list[j] == 'sch_uc':
                target_result_list[i][j] = target_parsed_list[i][key_list.index('sch_uc')]

            elif result_list[j] == 'UTC':
                target_result_list[i][j] = int(target_parsed_list[i][key_list.index('sch_bc_upa_tx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_uc_upa_tx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_pp_upa_tx')])
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
            elif result_list[j] == 'URC':
                target_result_list[i][j] = int(target_parsed_list[i][key_list.index('sch_bc_upa_rx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_uc_upa_rx')]) + \
                                                int(target_parsed_list[i][key_list.index('sch_pp_upa_rx')])
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


# Parse latency
# List to store latency result
latency_list = ['upward_per_hop_latency_count', 'upward_per_hop_latency_sum', 
                'downward_per_hop_latency_count', 'downward_per_hop_latency_sum', 
                'max_upward_per_hop_latency_in_slots', 'max_downward_per_hop_latency_in_slots']
LATENCY_NUM = len(latency_list)

bootstrap_topology_opt_latency_list = [[0 for i in range(LATENCY_NUM)] for j in range(NODE_NUM)]
bootstrap_traffic_opt_latency_list = [[0 for i in range(LATENCY_NUM)] for j in range(NODE_NUM)]
data_latency_list = [[0 for i in range(LATENCY_NUM)] for j in range(NODE_NUM)]

print()
print("Parse latency result")

if EVAL_CONFIG_WITH_SLA:
    # If SLA is applied, first parse application asn
    sla_trig_asn_list = [[1, 10]]

    file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + str(ROOT_ID) + '.txt'
    f = open(file_name, 'r', errors='ignore')
    print(file_name)

    line = f.readline()
    while line:
        if len(line) > 1:
            line = line.replace('\n', '')
            line_prefix = line.split(':')[0]
            if line_prefix == '[HK-S':
                line_body = line.split('] ')[1].split(' ')
                if line_body[0] == 'det':
                    if line_body[1] == 'c_ref_bc':
                        if line_body[line_body.index('c_ts') + 1] != line_body[line_body.index('n_ts') + 1]:
                            sla_trig_asn_list.append([int(line_body[line_body.index('t_asn') + 1], 16), float(line_body[line_body.index('n_ts') + 1]) / 1000])
        line = f.readline()
    f.close()

    # First record upward per hop latency
    file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + str(ROOT_ID) + '.txt'
    f = open(file_name, 'r', errors='ignore')
    print(file_name)

    target_latency_list = bootstrap_topology_opt_latency_list

    line = f.readline()
    while line:
        if len(line) > 1:
            line = line.replace('\n', '')
            line_prefix = line.split(':')[0]
            if line_prefix == '[HK-P':
                line_body = line.split('] ')[1].split(' ')
                if line_body[0] == 'reset_eval':
                    if line_body[1] == '0':
                        target_latency_list = bootstrap_traffic_opt_latency_list
                    elif line_body[1] == '1':
                        target_latency_list = data_latency_list
                if line_body[0] == 'rx_up':
                    lt_up_from_id = int(line_body[line_body.index('from') + 1])
                    lt_up_from_index = lt_up_from_id - 1

                    lt_up_t_asn = int(line_body[line_body.index('lt_up_t') + 1], 16)
                    lt_up_r_asn = int(line_body[line_body.index('lt_up_r') + 1], 16)
                    lt_up_hops = int(line_body[line_body.index('hops') + 1])

                    lt_up_t_asn_index = 0
                    for i in range(0, len(sla_trig_asn_list)):
                        if i < len(sla_trig_asn_list) - 1:
                            if sla_trig_asn_list[i][0] <= lt_up_t_asn and lt_up_t_asn < sla_trig_asn_list[i + 1][0]:
                                lt_up_t_asn_index = i
                                break
                        else:
                            lt_up_t_asn_index = i
                            break

                    lt_up_r_asn_index = 0
                    for i in range(0, len(sla_trig_asn_list)):
                        if i < len(sla_trig_asn_list) - 1:
                            if sla_trig_asn_list[i][0] <= lt_up_r_asn and lt_up_r_asn < sla_trig_asn_list[i + 1][0]:
                                lt_up_r_asn_index = i
                                break
                        else:
                            lt_up_r_asn_index = i
                            break

                    lt_up_ms = 0
                    lt_up_asn_index_diff = lt_up_r_asn_index - lt_up_t_asn_index

                    if lt_up_asn_index_diff == 0:
                        lt_up_ms += (lt_up_r_asn - lt_up_t_asn) * sla_trig_asn_list[lt_up_t_asn_index][1]
                    else:
                        lt_up_ms += (sla_trig_asn_list[lt_up_t_asn_index + 1][0] - lt_up_t_asn) * sla_trig_asn_list[lt_up_t_asn_index][1]
                        j = 1
                        while (lt_up_t_asn_index + j) < lt_up_r_asn_index:
                            lt_up_ms += (sla_trig_asn_list[lt_up_t_asn_index + j + 1][0]- sla_trig_asn_list[lt_up_t_asn_index + j][0]) * sla_trig_asn_list[lt_up_t_asn_index + j][1]
                            j += 1
                        lt_up_ms += (lt_up_r_asn - sla_trig_asn_list[lt_up_t_asn_index + j][0]) * sla_trig_asn_list[lt_up_t_asn_index + j][1]

                    lt_up_ms_per_hop = lt_up_ms / lt_up_hops

                    target_latency_list[lt_up_from_index][latency_list.index('upward_per_hop_latency_count')] += 1
                    target_latency_list[lt_up_from_index][latency_list.index('upward_per_hop_latency_sum')] += lt_up_ms_per_hop

                    lt_up_asn_diff = lt_up_r_asn - lt_up_t_asn
                    lt_up_asn_diff_per_hop = round(lt_up_asn_diff / lt_up_hops)
                    if lt_up_asn_diff_per_hop > target_latency_list[lt_up_from_index][latency_list.index('max_upward_per_hop_latency_in_slots')]:
                        target_latency_list[lt_up_from_index][latency_list.index('max_upward_per_hop_latency_in_slots')] = lt_up_asn_diff_per_hop

        line = f.readline()
    f.close()

    # Next record downward per hop latency
    for node_index in range(1, NODE_NUM):
        node_id = node_index + 1
        file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + str(node_id) + '.txt'
        f = open(file_name, 'r', errors='ignore')
        print(file_name)

        target_latency_list = bootstrap_topology_opt_latency_list

        line = f.readline()
        while line:
            if len(line) > 1:
                line = line.replace('\n', '')
                line_prefix = line.split(':')[0]
                if line_prefix == '[HK-P':
                    line_body = line.split('] ')[1].split(' ')
                    if line_body[0] == 'reset_eval':
                        if line_body[1] == '0':
                            target_latency_list = bootstrap_traffic_opt_latency_list
                        elif line_body[1] == '1':
                            target_latency_list = data_latency_list
                    if line_body[0] == 'rx_down':
                        lt_down_to_index = node_index
                        lt_down_to_id = node_index + 1

                        lt_down_t_asn = int(line_body[line_body.index('lt_down_t') + 1], 16)
                        lt_down_r_asn = int(line_body[line_body.index('lt_down_r') + 1], 16)
                        lt_down_hops = int(line_body[line_body.index('hops') + 1])

                        lt_down_t_asn_index = 0
                        for i in range(0, len(sla_trig_asn_list)):
                            if i < len(sla_trig_asn_list) - 1:
                                if sla_trig_asn_list[i][0] <= lt_down_t_asn and lt_down_t_asn < sla_trig_asn_list[i + 1][0]:
                                    lt_down_t_asn_index = i
                                    break
                            else:
                                lt_down_t_asn_index = i
                                break

                        lt_down_r_asn_index = 0
                        for i in range(0, len(sla_trig_asn_list)):
                            if i < len(sla_trig_asn_list) - 1:
                                if sla_trig_asn_list[i][0] <= lt_down_r_asn and lt_down_r_asn < sla_trig_asn_list[i + 1][0]:
                                    lt_down_r_asn_index = i
                                    break
                            else:
                                lt_down_r_asn_index = i
                                break

                        lt_down_ms = 0
                        lt_down_asn_index_diff = lt_down_r_asn_index - lt_down_t_asn_index

                        if lt_down_asn_index_diff == 0:
                            lt_down_ms += (lt_down_r_asn - lt_down_t_asn) * sla_trig_asn_list[lt_down_t_asn_index][1]
                        else:
                            lt_down_ms += (sla_trig_asn_list[lt_down_t_asn_index + 1][0] - lt_down_t_asn) * sla_trig_asn_list[lt_down_t_asn_index][1]
                            j = 1
                            while (lt_down_t_asn_index + j) < lt_down_r_asn_index:
                                lt_down_ms += (sla_trig_asn_list[lt_down_t_asn_index + j + 1][0]- sla_trig_asn_list[lt_down_t_asn_index + j][0]) * sla_trig_asn_list[lt_down_t_asn_index + j][1]
                                j += 1
                            lt_down_ms += (lt_down_r_asn - sla_trig_asn_list[lt_down_t_asn_index + j][0]) * sla_trig_asn_list[lt_down_t_asn_index + j][1]

                        lt_down_ms_per_hop = lt_down_ms / lt_down_hops

                        target_latency_list[lt_down_to_index][latency_list.index('downward_per_hop_latency_count')] += 1
                        target_latency_list[lt_down_to_index][latency_list.index('downward_per_hop_latency_sum')] += lt_down_ms_per_hop

                        lt_down_asn_diff = lt_down_r_asn - lt_down_t_asn
                        lt_down_asn_diff_per_hop = round(lt_down_asn_diff / lt_down_hops)
                        if lt_down_asn_diff_per_hop > target_latency_list[lt_down_to_index][latency_list.index('max_downward_per_hop_latency_in_slots')]:
                            target_latency_list[lt_down_to_index][latency_list.index('max_downward_per_hop_latency_in_slots')] = lt_down_asn_diff_per_hop

            line = f.readline()
        f.close()

else: # EVAL_CONFIG_WITH_SLA == 0
    DEFAULT_SLOT_LEN = 10

    # First record upward per hop latency
    file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + str(ROOT_ID) + '.txt'
    f = open(file_name, 'r', errors='ignore')
    print(file_name)

    target_latency_list = bootstrap_topology_opt_latency_list

    line = f.readline()
    while line:
        if len(line) > 1:
            line = line.replace('\n', '')
            line_prefix = line.split(':')[0]
            if line_prefix == '[HK-P':
                line_body = line.split('] ')[1].split(' ')
                if line_body[0] == 'reset_eval':
                    if line_body[1] == '0':
                        target_latency_list = bootstrap_traffic_opt_latency_list
                    elif line_body[1] == '1':
                        target_latency_list = data_latency_list
                if line_body[0] == 'rx_up':
                    lt_up_from_id = int(line_body[line_body.index('from') + 1])
                    lt_up_from_index = lt_up_from_id - 1

                    lt_up_t_asn = int(line_body[line_body.index('lt_up_t') + 1], 16)
                    lt_up_r_asn = int(line_body[line_body.index('lt_up_r') + 1], 16)
                    lt_up_hops = int(line_body[line_body.index('hops') + 1])

                    lt_up_ms = (lt_up_r_asn - lt_up_t_asn) * DEFAULT_SLOT_LEN

                    lt_up_ms_per_hop = lt_up_ms / lt_up_hops

                    target_latency_list[lt_up_from_index][latency_list.index('upward_per_hop_latency_count')] += 1
                    target_latency_list[lt_up_from_index][latency_list.index('upward_per_hop_latency_sum')] += lt_up_ms_per_hop

                    lt_up_asn_diff = lt_up_r_asn - lt_up_t_asn
                    lt_up_asn_diff_per_hop = round(lt_up_asn_diff / lt_up_hops)
                    if lt_up_asn_diff_per_hop > target_latency_list[lt_up_from_index][latency_list.index('max_upward_per_hop_latency_in_slots')]:
                        target_latency_list[lt_up_from_index][latency_list.index('max_upward_per_hop_latency_in_slots')] = lt_up_asn_diff_per_hop

        line = f.readline()
    f.close()

    # Next record downward per hop latency
    for node_index in range(1, NODE_NUM):
        node_id = node_index + 1
        file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + str(node_id) + '.txt'
        f = open(file_name, 'r', errors='ignore')
        print(file_name)

        target_latency_list = bootstrap_topology_opt_latency_list

        line = f.readline()
        while line:
            if len(line) > 1:
                line = line.replace('\n', '')
                line_prefix = line.split(':')[0]
                if line_prefix == '[HK-P':
                    line_body = line.split('] ')[1].split(' ')
                    if line_body[0] == 'reset_eval':
                        if line_body[1] == '0':
                            target_latency_list = bootstrap_traffic_opt_latency_list
                        elif line_body[1] == '1':
                            target_latency_list = data_latency_list
                    if line_body[0] == 'rx_down':
                        lt_down_to_index = node_index
                        lt_down_to_id = node_index + 1

                        lt_down_t_asn = int(line_body[line_body.index('lt_down_t') + 1], 16)
                        lt_down_r_asn = int(line_body[line_body.index('lt_down_r') + 1], 16)
                        lt_down_hops = int(line_body[line_body.index('hops') + 1])

                        lt_down_ms = (lt_down_r_asn - lt_down_t_asn) * DEFAULT_SLOT_LEN

                        lt_down_ms_per_hop = lt_down_ms / lt_down_hops

                        target_latency_list[lt_down_to_index][latency_list.index('downward_per_hop_latency_count')] += 1
                        target_latency_list[lt_down_to_index][latency_list.index('downward_per_hop_latency_sum')] += lt_down_ms_per_hop

                        lt_down_asn_diff = lt_down_r_asn - lt_down_t_asn
                        lt_down_asn_diff_per_hop = round(lt_down_asn_diff / lt_down_hops)
                        if lt_down_asn_diff_per_hop > target_latency_list[lt_down_to_index][latency_list.index('max_downward_per_hop_latency_in_slots')]:
                            target_latency_list[lt_down_to_index][latency_list.index('max_downward_per_hop_latency_in_slots')] = lt_down_asn_diff_per_hop

            line = f.readline()
        f.close()

for (target_latency_list, target_result_list) in [(bootstrap_topology_opt_latency_list, bootstrap_period_topology_opt_result), \
                                                  (bootstrap_traffic_opt_latency_list, bootstrap_period_traffic_opt_result), \
                                                  (data_latency_list, data_period_result)]:
    for i in range(NODE_NUM):
        if float(target_latency_list[i][latency_list.index('upward_per_hop_latency_count')]) == 0:
            target_result_list[i][result_list.index('uLT')] = 'NaN'
        else:
            target_result_list[i][result_list.index('uLT')] = round(float(target_latency_list[i][latency_list.index('upward_per_hop_latency_sum')]) / float(target_latency_list[i][latency_list.index('upward_per_hop_latency_count')]), 1)
        if float(target_latency_list[i][latency_list.index('downward_per_hop_latency_count')]) == 0:
            target_result_list[i][result_list.index('dLT')] = 'NaN'
        else:
            target_result_list[i][result_list.index('dLT')] = round(float(target_latency_list[i][latency_list.index('downward_per_hop_latency_sum')]) / float(target_latency_list[i][latency_list.index('downward_per_hop_latency_count')]), 1)
        if (float(target_latency_list[i][latency_list.index('upward_per_hop_latency_count')]) + float(target_latency_list[i][latency_list.index('downward_per_hop_latency_count')])) == 0:
            target_result_list[i][result_list.index('LT')] = 'NaN'
        else:
            target_result_list[i][result_list.index('LT')] = round((float(target_latency_list[i][latency_list.index('upward_per_hop_latency_sum')]) + float(target_latency_list[i][latency_list.index('downward_per_hop_latency_sum')])) / (float(target_latency_list[i][latency_list.index('upward_per_hop_latency_count')]) + float(target_latency_list[i][latency_list.index('downward_per_hop_latency_count')])), 1)
        target_result_list[i][result_list.index('MuLT')] = target_latency_list[i][latency_list.index('max_upward_per_hop_latency_in_slots')]
        target_result_list[i][result_list.index('MdLT')] = target_latency_list[i][latency_list.index('max_downward_per_hop_latency_in_slots')]


# Parse slot length
# List to store slot length result
print()
print("Parse slot length")

slot_len_list = ['slot_len_count', 'slot_len_ms_sum']
SLOT_LEN_NUM = len(slot_len_list)

bootstrap_topology_opt_slot_len_list = [[0 for i in range(SLOT_LEN_NUM)] for j in range(NODE_NUM)]
bootstrap_traffic_opt_slot_len_list = [[0 for i in range(SLOT_LEN_NUM)] for j in range(NODE_NUM)]
data_slot_len_list = [[0 for i in range(SLOT_LEN_NUM)] for j in range(NODE_NUM)]

for node_index in range(0, NODE_NUM):
    node_id = node_index + 1
    file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + str(node_id) + '.txt'
    f = open(file_name, 'r', errors='ignore')
    print(file_name)

    target_slot_len_list = bootstrap_topology_opt_slot_len_list

    line = f.readline()
    while line:
        if len(line) > 1:
            line = line.replace('\n', '')
            line_prefix = line.split(':')[0]
            line_postfix = line.split(' ')[-1]
            if line_prefix == '[HK-P':
                line_body = line.split('] ')[1].split(' ')
                if line_body[0] == 'reset_eval':
                    if line_body[1] == '0':
                        target_slot_len_list = bootstrap_traffic_opt_slot_len_list
                    elif line_body[1] == '1':
                        target_slot_len_list = data_slot_len_list
            if line_postfix == 'HK-T':
                line_body = line.split('] ')[1].split(' ')
                if line_body[-3] != '0':
                    current_slot_len = float(line_body[-3]) / 1000
                    target_slot_len_list[node_index][slot_len_list.index('slot_len_count')] += 1
                    target_slot_len_list[node_index][slot_len_list.index('slot_len_ms_sum')] += current_slot_len

        line = f.readline()
    f.close()

for (target_slot_len_list, target_result_list) in [(bootstrap_topology_opt_slot_len_list, bootstrap_period_topology_opt_result), \
                                                   (bootstrap_traffic_opt_slot_len_list, bootstrap_period_traffic_opt_result), \
                                                   (data_slot_len_list, data_period_result)]:
    for i in range(NODE_NUM):
        if float(target_slot_len_list[i][slot_len_list.index('slot_len_count')]) == 0:
            target_result_list[i][result_list.index('ts_l')] = 'NaN'
        else:
            target_result_list[i][result_list.index('ts_l')] = round(float(target_slot_len_list[i][slot_len_list.index('slot_len_ms_sum')]) / float(target_slot_len_list[i][slot_len_list.index('slot_len_count')]), 2)


# Print parsed result
SPLIT_KEY = 'as_ts'

print()
for target_result_list in [bootstrap_period_topology_opt_result, bootstrap_period_traffic_opt_result, data_period_result]:
    if target_result_list == bootstrap_period_topology_opt_result:
        print('----- Topology optimization period -----')
    elif target_result_list == bootstrap_period_traffic_opt_result:
        print('----- Traffic optimization period -----')
    else:
        print('----- Data period -----')

    if show_all == '0':
        for i in range(result_list.index(SPLIT_KEY) - 1):
            print(result_list[i], '\t', end='')
        print(result_list[result_list.index(SPLIT_KEY) - 1])
        for i in range(NODE_NUM):
            for j in range(result_list.index(SPLIT_KEY) - 1):
                print(target_result_list[i][j], '\t', end='')
            print(target_result_list[i][result_list.index(SPLIT_KEY) - 1])
        print()

    elif show_all == '1':
        for i in range(result_list.index(SPLIT_KEY) - 1):
            print(result_list[i], '\t', end='')
        print(result_list[result_list.index(SPLIT_KEY) - 1])
        for i in range(NODE_NUM):
            for j in range(result_list.index(SPLIT_KEY) - 1):
                print(target_result_list[i][j], '\t', end='')
            print(target_result_list[i][result_list.index(SPLIT_KEY) - 1])
        print()
        for i in range(result_list.index(SPLIT_KEY), RESULT_NUM - 1): 
            print(result_list[i], '\t', end='')
        print(result_list[-1], '\t', end='')
        print(result_list[0])
        for i in range(NODE_NUM):
            for j in range(result_list.index(SPLIT_KEY), RESULT_NUM - 1): 
                print(target_result_list[i][j], '\t', end='')
            print(target_result_list[i][-1], '\t', end='')
            print(target_result_list[i][0])
        print()

    if root_node_bootstrap_finished == 0:
        print('----- In topology optimization period -----')
        print()
        break
    elif root_node_bootstrap_finished == 1:
        print('----- In traffic optimization period -----')
        print()
        break



output_file_name = 'result.txt'
o = open(output_file_name, 'w') 

for i in range(RESULT_NUM - 1):
    output_data = str(result_list[i]) + '\t'
    o.write(output_data)
output_data = str(result_list[-1]) + '\n'
o.write(output_data)

for i in range(NODE_NUM):
    for j in range(RESULT_NUM - 1):
        output_data = str(data_period_result[i][j]) + '\t'
        o.write(output_data)
    output_data = str(data_period_result[i][-1]) + '\n'
    o.write(output_data)
o.close()


# Print summarized result
all_continuity_ok = 1
all_bootP_ok = 1
all_bootQ_ok = 1

for i in range(1, NODE_NUM):
    if data_period_parsed[i][key_list.index('logging_disrupted')] == 1:
        all_continuity_ok = 0
    if data_period_parsed[i][key_list.index('opku')] == 0:
        all_bootP_ok = 0
    if data_period_parsed[i][key_list.index('rs_q')] > 1:
        all_bootQ_ok = 0

print('----- Evaluation configuration -----')
print('FIXED_TOPOLOGY: ' + str(EVAL_CONFIG_FIXED_TOPOLOGY))
print('LITE_LOG: ' + str(EVAL_CONFIG_LITE_LOG))
print('TRAFFIC_LOAD: ' + str(EVAL_CONFIG_TRAFFIC_LOAD))
print('DOWN_TRAFFIC_LOAD: ' + str(EVAL_CONFIG_DOWN_TRAFFIC_LOAD))
print('APP_PAYLOAD_LEN: ' + str(EVAL_CONFIG_APP_PAYLOAD_LEN))
print('SLOT_LEN: ' + str(EVAL_CONFIG_SLOT_LEN))
print('UCSF_PERIOD: ' + str(EVAL_CONFIG_UCSF_PERIOD))
print('WITH_UPA: ' + str(EVAL_CONFIG_WITH_UPA))
print('WITH_SLA: ' + str(EVAL_CONFIG_WITH_SLA) + ' (k: ' + str(EVAL_CONFIG_SLA_K) + ')')
print('WITH_DBT: ' + str(EVAL_CONFIG_WITH_DBT))
print('WITH_A3: ' + str(EVAL_CONFIG_WITH_A3) + ' (max zone: ' + str(EVAL_CONFIG_A3_MAX_ZONE) + ')')
print('Continuity: ' + str(all_continuity_ok))
print('BootP: ' + str(all_bootP_ok))
print('BootQ: ' + str(all_bootQ_ok))

output_file_name = 'summary.txt'
o = open(output_file_name, 'w')
output_data = 'FIXED_TOPOLOGY: ' + str(EVAL_CONFIG_FIXED_TOPOLOGY) + '\n'
o.write(output_data)
output_data = 'LITE_LOG: ' + str(EVAL_CONFIG_LITE_LOG) + '\n'
o.write(output_data)
output_data = 'TRAFFIC_LOAD: ' + str(EVAL_CONFIG_TRAFFIC_LOAD) + '\n'
o.write(output_data)
output_data = 'DOWN_TRAFFIC_LOAD: ' + str(EVAL_CONFIG_DOWN_TRAFFIC_LOAD) + '\n'
o.write(output_data)
output_data = 'APP_PAYLOAD_LEN: ' + str(EVAL_CONFIG_APP_PAYLOAD_LEN) + '\n'
o.write(output_data)
output_data = 'SLOT_LEN: ' + str(EVAL_CONFIG_SLOT_LEN) + '\n'
o.write(output_data)
output_data = 'UCSF_PERIOD: ' + str(EVAL_CONFIG_UCSF_PERIOD) + '\n'
o.write(output_data)
output_data = 'WITH_UPA: ' + str(EVAL_CONFIG_WITH_UPA) + '\n'
o.write(output_data)
output_data = 'WITH_SLA: ' + str(EVAL_CONFIG_WITH_SLA) + ' (k: ' + str(EVAL_CONFIG_SLA_K) + ')' + '\n'
o.write(output_data)
output_data = 'WITH_DBT: ' + str(EVAL_CONFIG_WITH_DBT) + '\n'
o.write(output_data)
output_data = 'WITH_A3: ' + str(EVAL_CONFIG_WITH_A3) + ' (max zone: ' + str(EVAL_CONFIG_A3_MAX_ZONE) + ')' + '\n'
o.write(output_data)
output_data = 'Continuity: ' + str(all_continuity_ok) + '\n'
o.write(output_data)
output_data = 'BootP: ' + str(all_bootP_ok) + '\n'
o.write(output_data)
output_data = 'BootQ: ' + str(all_bootQ_ok) + '\n'
o.write(output_data)
o.close()