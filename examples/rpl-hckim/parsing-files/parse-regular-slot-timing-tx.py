import argparse

parser = argparse.ArgumentParser()
parser.add_argument('any_iter')
parser.add_argument('any_id')
args = parser.parse_args()

any_iter = args.any_iter
any_id = args.any_id

minimum_len = 40
maximum_len = 125
timing_len = 18
regular_slot_timing = [[0 for col in range(timing_len)] for row in range(maximum_len - minimum_len + 1)]
regular_slot_timing_updated = [0 for row in range(maximum_len - minimum_len + 1)]


file_name = 'log-' + any_iter + '-' + any_id + '.txt'
f = open(file_name, 'r', errors='ignore')

line = f.readline()
while line:
    if len(line) > 1:
        line = line.replace('\n', '')
        line_s = line.split('] ')
        if len(line_s) > 1:
            message = line_s[1].split(' ')
            if message[0] == '{asn':
                message_s = line.split('} ')
                contents = message_s[1].split(' ')
                if contents[0] == 'reg' and contents[1] == 't_r':
                    if contents[3] == '1' and contents[4] == '0':
                        current_len = int(contents[2])
                        if current_len >= minimum_len:
                            current_len_index = current_len - minimum_len
                            if regular_slot_timing_updated[current_len_index] == 0:
                                regular_slot_timing_updated[current_len_index] = 1
                            elif regular_slot_timing_updated[current_len_index] == 1:
                                regular_slot_timing_updated[current_len_index] = 2
                                line_t = f.readline()
                                if len(line_t) > 1:
                                    line_t = line_t.replace('\n', '')
                                    message_t_s = line_t.split('} ')
                                    contents_t = message_t_s[1].split(' ')
                                    regular_slot_timing[current_len_index][0] = contents_t[2]
                                    regular_slot_timing[current_len_index][1] = contents_t[3]
                                line_t = f.readline()
                                if len(line_t) > 1:
                                    line_t = line_t.replace('\n', '')
                                    message_t_s = line_t.split('} ')
                                    contents_t = message_t_s[1].split(' ')
                                    regular_slot_timing[current_len_index][2] = contents_t[2]
                                    regular_slot_timing[current_len_index][3] = contents_t[3]
                                    regular_slot_timing[current_len_index][4] = contents_t[4]
                                    regular_slot_timing[current_len_index][5] = contents_t[5]
                                line_t = f.readline()
                                if len(line_t) > 1:
                                    line_t = line_t.replace('\n', '')
                                    message_t_s = line_t.split('} ')
                                    contents_t = message_t_s[1].split(' ')
                                    regular_slot_timing[current_len_index][6] = contents_t[2]
                                    regular_slot_timing[current_len_index][7] = contents_t[3]
                                    regular_slot_timing[current_len_index][8] = contents_t[4]
                                    regular_slot_timing[current_len_index][9] = contents_t[5]
                                line_t = f.readline()
                                if len(line_t) > 1:
                                    line_t = line_t.replace('\n', '')
                                    message_t_s = line_t.split('} ')
                                    contents_t = message_t_s[1].split(' ')
                                    regular_slot_timing[current_len_index][10] = contents_t[2]
                                    regular_slot_timing[current_len_index][11] = contents_t[3]
                                    regular_slot_timing[current_len_index][12] = contents_t[4]
                                    regular_slot_timing[current_len_index][13] = contents_t[5]
                                line_t = f.readline()
                                if len(line_t) > 1:
                                    line_t = line_t.replace('\n', '')
                                    message_t_s = line_t.split('} ')
                                    contents_t = message_t_s[1].split(' ')
                                    regular_slot_timing[current_len_index][14] = contents_t[2]
                                    regular_slot_timing[current_len_index][15] = contents_t[3]
                                    regular_slot_timing[current_len_index][16] = contents_t[4]
                                    regular_slot_timing[current_len_index][17] = contents_t[5]
    line = f.readline()
f.close()

for i in range(maximum_len - minimum_len + 1):
    print(str(i + minimum_len), '\t', end='')
    for j in range(timing_len - 1):
        print(str(regular_slot_timing[i][j]), '\t', end='')
    print(regular_slot_timing[i][timing_len - 1])
