import argparse
from sys import base_exec_prefix

HCK = 0
IND = 1
VAL = 2
NID = 4
ADDR = 5

ROOT_ID = 1
ROOT_ADDR = 'NULL'
non_root_id_list = list()
non_root_address_list = list()

parser = argparse.ArgumentParser()
parser.add_argument('any_scheduler')
parser.add_argument('any_iter')
parser.add_argument('any_id')
args = parser.parse_args()

any_scheduler = args.any_scheduler
any_iter = args.any_iter
any_id = args.any_id

print('----- evaluation info -----')
print('any_scheduler: ' + any_scheduler)
print('any_iter: ' + any_iter)
print('any_id: ' + any_id)
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
                elif message[1] == 'non_root':
                    non_root_id_list.append(int(message[2]))
                    non_root_address_list.append(message[3])
                elif message[1] == 'end':
                    break
    line = f.readline()
f.close()

NODE_NUM = 1 + len(non_root_id_list) # root + non-root nodes

print('----- node info -----')
print('root id: ', ROOT_ID)
print('non-root ids: ', non_root_id_list)
print()

case_list = ['A', 'A_t', 'A_t_o', 'A_t_n', 'A_r', 'bc', 'bc_t', 'bc_r', 'uc', 'uc_t', 'uc_t_o', 'uc_t_n', 'uc_r']
CASE_NUM = len(case_list)

case_parsed = [[0 for i in range(CASE_NUM)] for j in range(NODE_NUM)]
case_time = [[0 for i in range(CASE_NUM)] for j in range(NODE_NUM)]
case_result = [[0 for i in range(CASE_NUM)] for j in range(NODE_NUM)]

for node_id in non_root_id_list:
    node_index = non_root_id_list.index(node_id) + 1 # index in parsed array

    file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + str(node_id) + '.txt'
    f = open(file_name, 'r', errors='ignore')
    in_data_period = 0

    line = f.readline()
    while line:
        if len(line) > 1:
            line = line.replace('\n', '')
            line_s = line.split('] ')
            if len(line_s) > 1:
                message = line_s[1].split(' ')
                if in_data_period == 0:
                    if message[HCK] == 'HCK':
                        current_metric = message[IND]
                        if current_metric == 'reset_log':
                            in_data_period = 1
                            line = f.readline()
                            continue
                if in_data_period == 1:
                    if message[0] == '{asn':
                        message_s = line.split('} ')
                        if len(message_s) > 1:
                            contents = message_s[1].split(' ')
                            app_data = 0
                            if 'a_seq' in contents:
                                app_data = 1
                            if app_data == 1:
                                case_parsed[node_index][case_list.index('A')] += 1
                                parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                case_time[node_index][case_list.index('A')] += parsed_idle
                                if contents[1] == 'tx':
                                    case_parsed[node_index][case_list.index('A_t')] += 1
                                    parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                    case_time[node_index][case_list.index('A_t')] += parsed_idle
                                    if contents[contents.index('st') + 1] == '0':
                                        case_parsed[node_index][case_list.index('A_t_o')] += 1
                                        parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                        case_time[node_index][case_list.index('A_t_o')] += parsed_idle
                                    else:
                                        case_parsed[node_index][case_list.index('A_t_n')] += 1
                                        parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                        case_time[node_index][case_list.index('A_t_n')] += parsed_idle
                                elif contents[1] == 'rx':
                                    case_parsed[node_index][case_list.index('A_r')] += 1
                                    parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                    case_time[node_index][case_list.index('A_r')] += parsed_idle                                
                            if contents[0] == 'bc-1-0':
                                case_parsed[node_index][case_list.index('bc')] += 1
                                parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                case_time[node_index][case_list.index('bc')] += parsed_idle
                                if contents[1] == 'tx':
                                    case_parsed[node_index][case_list.index('bc_t')] += 1
                                    parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                    case_time[node_index][case_list.index('bc_t')] += parsed_idle
                                elif contents[1] == 'rx':
                                    case_parsed[node_index][case_list.index('bc_r')] += 1
                                    parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                    case_time[node_index][case_list.index('bc_r')] += parsed_idle
                            elif contents[0] == 'uc-1-0':
                                case_parsed[node_index][case_list.index('uc')] += 1
                                parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                case_time[node_index][case_list.index('uc')] += parsed_idle
                                if contents[1] == 'tx':
                                    case_parsed[node_index][case_list.index('uc_t')] += 1
                                    parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                    case_time[node_index][case_list.index('uc_t')] += parsed_idle
                                    if contents[contents.index('st') + 1] == '0':
                                        case_parsed[node_index][case_list.index('uc_t_o')] += 1
                                        parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                        case_time[node_index][case_list.index('uc_t_o')] += parsed_idle
                                    else:
                                        case_parsed[node_index][case_list.index('uc_t_n')] += 1
                                        parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                        case_time[node_index][case_list.index('uc_t_n')] += parsed_idle
                                elif contents[1] == 'rx':
                                    case_parsed[node_index][case_list.index('uc_r')] += 1
                                    parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                    case_time[node_index][case_list.index('uc_r')] += parsed_idle
        line = f.readline()
    f.close()

ROOT_INDEX = 0

file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + str(ROOT_ID) + '.txt'
f = open(file_name, 'r', errors='ignore')
in_data_period = 0

line = f.readline()
while line:
    if len(line) > 1:
        line = line.replace('\n', '')
        line_s = line.split('] ')
        if len(line_s) > 1:
            message = line_s[1].split(' ')
            if in_data_period == 0:
                if message[HCK] == 'HCK':
                    current_metric = message[IND]
                    if current_metric == 'reset_log':
                        in_data_period = 1
                        line = f.readline()
                        continue
            else:
                if message[0] == '{asn':
                    message_s = line.split('} ')
                    contents = message_s[1].split(' ')
                    app_data = 0
                    if 'a_seq' in contents:
                        app_data = 1
                    if app_data == 1:
                        case_parsed[ROOT_INDEX][case_list.index('A')] += 1
                        parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                        case_time[ROOT_INDEX][case_list.index('A')] += parsed_idle
                        if contents[1] == 'tx':
                            case_parsed[ROOT_INDEX][case_list.index('A_t')] += 1
                            parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                            case_time[ROOT_INDEX][case_list.index('A_t')] += parsed_idle
                            if contents[contents.index('st') + 1] == '0':
                                case_parsed[ROOT_INDEX][case_list.index('A_t_o')] += 1
                                parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                case_time[ROOT_INDEX][case_list.index('A_t_o')] += parsed_idle
                            else:
                                case_parsed[ROOT_INDEX][case_list.index('A_t_n')] += 1
                                parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                case_time[ROOT_INDEX][case_list.index('A_t_n')] += parsed_idle
                        elif contents[1] == 'rx':
                            case_parsed[ROOT_INDEX][case_list.index('A_r')] += 1
                            parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                            case_time[ROOT_INDEX][case_list.index('A_r')] += parsed_idle     
                    if contents[0] == 'bc-1-0':
                        case_parsed[ROOT_INDEX][case_list.index('bc')] += 1
                        parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                        case_time[ROOT_INDEX][case_list.index('bc')] += parsed_idle
                        if contents[1] == 'tx':
                            case_parsed[ROOT_INDEX][case_list.index('bc_t')] += 1
                            parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                            case_time[ROOT_INDEX][case_list.index('bc_t')] += parsed_idle
                        elif contents[1] == 'rx':
                            case_parsed[ROOT_INDEX][case_list.index('bc_r')] += 1
                            parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                            case_time[ROOT_INDEX][case_list.index('bc_r')] += parsed_idle
                    elif contents[0] == 'uc-1-0':
                        case_parsed[ROOT_INDEX][case_list.index('uc')] += 1
                        parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                        case_time[ROOT_INDEX][case_list.index('uc')] += parsed_idle
                        if contents[1] == 'tx':
                            case_parsed[ROOT_INDEX][case_list.index('uc_t')] += 1
                            parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                            case_time[ROOT_INDEX][case_list.index('uc_t')] += parsed_idle
                            if contents[contents.index('st') + 1] == '0':
                                case_parsed[ROOT_INDEX][case_list.index('uc_t_o')] += 1
                                parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                case_time[ROOT_INDEX][case_list.index('uc_t_o')] += parsed_idle
                            else:
                                case_parsed[ROOT_INDEX][case_list.index('uc_t_n')] += 1
                                parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                                case_time[ROOT_INDEX][case_list.index('uc_t_n')] += parsed_idle
                        elif contents[1] == 'rx':
                            case_parsed[ROOT_INDEX][case_list.index('uc_r')] += 1
                            parsed_idle = int(contents[contents.index('idle') + 1].split(',')[0])
                            case_time[ROOT_INDEX][case_list.index('uc_r')] += parsed_idle
    line = f.readline()

f.close()

for i in range(NODE_NUM):
    for j in range(CASE_NUM):
        if case_parsed[i][j] == 0:
            case_result[i][j] = 'NaN'
        else:
            case_result[i][j] = round(case_time[i][j] / case_parsed[i][j], 1)

print('----- Cases -----')
#for i in range(CASE_NUM - 1):
#    print(case_list[i], '\t', end='')
#print(case_list[CASE_NUM - 1], '\t', end='')
for i in range(CASE_NUM - 1):
    print(case_list[i], '\t', end='')
print(case_list[CASE_NUM - 1])

for i in range(NODE_NUM):
#    for j in range(CASE_NUM - 1):
#        print(case_parsed[i][j], '\t', end='')
#    print(case_parsed[i][CASE_NUM - 1], '\t', end='')
    for j in range(CASE_NUM - 1):
        print(case_result[i][j], '\t', end='')
    print(case_result[i][CASE_NUM - 1])
print()
