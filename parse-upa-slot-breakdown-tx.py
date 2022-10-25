import argparse

parser = argparse.ArgumentParser()
parser.add_argument('any_scheduler')
parser.add_argument('any_iter')
parser.add_argument('any_id')
parser.add_argument('pkt_count')
args = parser.parse_args()

any_scheduler = args.any_scheduler
any_iter = args.any_iter
any_id = args.any_id
pkt_count = args.pkt_count

minimum_len = 59
maximum_len = 125

required_cols = [42, 46, 50, 54, 58, 62]

upa_timing = [[0 for col in range(required_cols[int(pkt_count) - 1])] for row in range(maximum_len - minimum_len + 1)]
upa_timing_updated = [0 for row in range(maximum_len - minimum_len + 1)]

current_timing = [0 for row in range(required_cols[int(pkt_count) - 1])]
current_is_upa_link = 0

file_name = 'log-' + any_scheduler + '-' + any_iter + '-' + any_id + '.txt'
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
                    current_len = int(contents[2])
                    if current_len >= minimum_len:
                        current_len_index = current_len - minimum_len
                        current_is_ack_required = int(contents[3])
                        current_tx_result = int(contents[4])
                        if current_is_ack_required == 1 and current_tx_result == 0:
                            line_t = f.readline()
                            if len(line_t) > 1:
                                line_t = line_t.replace('\n', '')
                                line_t_s = line_t.split('} ')
                                contents_t = line_t_s[1].split(' ')
                                if contents_t[0] == 'reg' and contents_t[1] == 't_c':
                                    current_timing[0] = contents_t[2]
                                    current_timing[1] = contents_t[3]
                            line_t = f.readline()
                            if len(line_t) > 1:
                                line_t = line_t.replace('\n', '')
                                line_t_s = line_t.split('} ')
                                contents_t = line_t_s[1].split(' ')
                                if contents_t[0] == 'reg' and contents_t[1] == 't_1':
                                    current_timing[2] = contents_t[2]
                                    current_timing[3] = contents_t[3]
                                    current_timing[4] = contents_t[4]
                                    current_timing[5] = contents_t[5]
                            line_t = f.readline()
                            if len(line_t) > 1:
                                line_t = line_t.replace('\n', '')
                                line_t_s = line_t.split('} ')
                                contents_t = line_t_s[1].split(' ')
                                if contents_t[0] == 'reg' and contents_t[1] == 't_2':
                                    current_timing[6] = contents_t[2]
                                    current_timing[13] = contents_t[3]
                                    current_timing[14] = contents_t[4]
                                    current_timing[15] = contents_t[5]
                            line_t = f.readline()
                            if len(line_t) > 1:
                                line_t = line_t.replace('\n', '')
                                line_t_s = line_t.split('} ')
                                contents_t = line_t_s[1].split(' ')
                                if contents_t[0] == 'reg' and contents_t[1] == 't_3':
                                    current_timing[16] = contents_t[2]
                                    current_timing[17] = contents_t[3]
                                    current_timing[18] = contents_t[4]
                                    current_timing[19] = contents_t[5]
                            line_t = f.readline()
                            if len(line_t) > 1:
                                line_t = line_t.replace('\n', '')
                                line_t_s = line_t.split('} ')
                                contents_t = line_t_s[1].split(' ')
                                if contents_t[0] == 'reg' and contents_t[1] == 't_4':
                                    current_timing[20] = contents_t[2]
                                    current_timing[21] = contents_t[3]
                                    current_timing[22] = contents_t[4]
                                    current_timing[23] = contents_t[5]
                            line_t = f.readline()
                            if len(line_t) > 1:
                                line_t = line_t.replace('\n', '')
                                line_t_s = line_t.split('} ')
                                contents_t = line_t_s[1].split(' ')
                                if contents_t[0] == 't_c':
                                    current_timing[7] = contents_t[1]
                                    current_timing[8] = contents_t[2]
                                    current_timing[9] = contents_t[3]
                                    current_timing[10] = contents_t[4]
                                    current_timing[11] = contents_t[5]
                                    current_timing[12] = contents_t[6]
                            line_t = f.readline()
                            if len(line_t) > 1:
                                line_t = line_t.replace('\n', '')
                                line_t_s = line_t.split('} ')
                                contents_t = line_t_s[1].split(' ')
                                if contents_t[0] == 'upa' and contents_t[1] == 't_r':
                                    upa_current_len = int(contents[2])
                                    if upa_current_len == current_len:
                                        all_same_len = int(contents_t[3])
                                        tx_count = int(contents_t[4])
                                        tx_ok = int(contents_t[5])
                                        if all_same_len == 1 and tx_count == tx_ok and tx_ok == int(pkt_count):
                                            if upa_timing_updated[current_len_index] == 0:
                                                upa_timing_updated[current_len_index] = 1
                                                line_t = f.readline()
                                                if len(line_t) > 1:
                                                    line_t = line_t.replace('\n', '')
                                                    line_t_s = line_t.split('} ')
                                                    contents_t = line_t_s[1].split(' ')
                                                    if contents_t[0] == 'upa' and contents_t[1] == 't_h':
                                                        current_timing[24] = contents_t[2]
                                                        current_timing[25] = contents_t[3]
                                                        current_timing[26] = contents_t[3]
                                                line_t = f.readline()
                                                if len(line_t) > 1:
                                                    line_t = line_t.replace('\n', '')
                                                    line_t_s = line_t.split('} ')
                                                    contents_t = line_t_s[1].split(' ')
                                                    if contents_t[0] == 'upa' and contents_t[1] == 't_b':
                                                        current_timing[27] = contents_t[2]
                                                        current_timing[28] = contents_t[3]
                                                for j in range(tx_ok):
                                                    line_t = f.readline()
                                                    if len(line_t) > 1:
                                                        line_t = line_t.replace('\n', '')
                                                        line_t_s = line_t.split('} ')
                                                        contents_t = line_t_s[1].split(' ')
                                                        if contents_t[0] == 'upa':
                                                            current_timing[28 + j * 4 + 1] = contents_t[2]
                                                            current_timing[28 + j * 4 + 2] = contents_t[3]
                                                            current_timing[28 + j * 4 + 3] = contents_t[4]
                                                            current_timing[28 + j * 4 + 4] = contents_t[5]
                                                line_t = f.readline()
                                                if len(line_t) > 1:
                                                    line_t = line_t.replace('\n', '')
                                                    line_t_s = line_t.split('} ')
                                                    contents_t = line_t_s[1].split(' ')
                                                    if contents_t[0] == 'upa' and contents_t[1] == 't_a':
                                                        current_timing[28 + tx_ok * 4 + 1] = contents_t[2]
                                                        current_timing[28 + tx_ok * 4 + 2] = contents_t[3]
                                                        current_timing[28 + tx_ok * 4 + 3] = contents_t[4]
                                                        current_timing[28 + tx_ok * 4 + 4] = contents_t[5]
                                                line_t = f.readline()
                                                if len(line_t) > 1:
                                                    line_t = line_t.replace('\n', '')
                                                    line_t_s = line_t.split('} ')
                                                    contents_t = line_t_s[1].split(' ')
                                                    if contents_t[0] == 'upa' and contents_t[1] == 't_e':
                                                        current_timing[28 + tx_ok * 4 + 5] = contents_t[2]
                                                        current_timing[28 + tx_ok * 4 + 6] = contents_t[3]
                                                        current_timing[28 + tx_ok * 4 + 7] = contents_t[4]
                                                line_t = f.readline()
                                                if len(line_t) > 1:
                                                    line_t = line_t.replace('\n', '')
                                                    line_t_s = line_t.split('} ')
                                                    contents_t = line_t_s[1].split(' ')
                                                    if contents_t[0] == 'asap' and contents_t[1] == 'c_e':
                                                        current_timing[28 + tx_ok * 4 + 8] = contents_t[2]
                                                        current_timing[28 + tx_ok * 4 + 9] = contents_t[3]

                                                for k in range(required_cols[int(pkt_count) - 1]):
                                                    upa_timing[current_len_index][k] = current_timing[k]
                                                    current_timing[k] = 0
    line = f.readline()
f.close()

for i in range(maximum_len - minimum_len + 1):
    print(str(i + minimum_len), '\t', end='')
    for j in range(required_cols[int(pkt_count) - 1] - 1):
        print(str(upa_timing[i][j]), '\t', end='')
    print(upa_timing[i][required_cols[int(pkt_count) - 1] - 1])
