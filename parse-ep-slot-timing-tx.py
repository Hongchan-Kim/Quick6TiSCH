import argparse

parser = argparse.ArgumentParser()
parser.add_argument('any_iter')
parser.add_argument('any_id')
parser.add_argument('pkt_count')
args = parser.parse_args()

any_iter = args.any_iter
any_id = args.any_id
pkt_count = args.pkt_count

minimum_len = 46
maximum_len = 125

required_cols = [13, 17, 21, 25, 29, 33]

ep_timing = [[0 for col in range(required_cols[int(pkt_count) - 1])] for row in range(maximum_len - minimum_len + 1)]
ep_timing_updated = [0 for row in range(maximum_len - minimum_len + 1)]


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
                if contents[0] == 'ep' and contents[1] == 't_r':
                    current_len = int(contents[2])
                    if current_len >= minimum_len:
                        current_len_index = current_len - minimum_len
                        all_same_len = int(contents[3])
                        tx_count = int(contents[4])
                        tx_ok = int(contents[5])
                        if all_same_len == 1 and tx_count == tx_ok:
                            if tx_ok == int(pkt_count):
                                if ep_timing_updated[current_len_index] == 0:
                                    ep_timing_updated[current_len_index] = 1
                                    line_t = f.readline()
                                    if len(line_t) > 1:
                                        line_t = line_t.replace('\n', '')
                                        line_t_s = line_t.split('} ')
                                        contents_t = line_t_s[1].split(' ')
                                        ep_timing[current_len_index][0] = contents_t[2]
                                        ep_timing[current_len_index][1] = contents_t[3]
                                    for i in range(tx_ok):
                                        line_t = f.readline()
                                        if len(line_t) > 1:
                                            line_t = line_t.replace('\n', '')
                                            line_t_s = line_t.split('} ')
                                            contents_t = line_t_s[1].split(' ')
                                            ep_timing[current_len_index][4 * i + 2] = contents_t[2]
                                            ep_timing[current_len_index][4 * i + 3] = contents_t[3]
                                            ep_timing[current_len_index][4 * i + 4] = contents_t[4]
                                            ep_timing[current_len_index][4 * i + 5] = contents_t[5]
                                    line_t = f.readline()
                                    if len(line_t) > 1:
                                        line_t = line_t.replace('\n', '')
                                        line_t_s = line_t.split('} ')
                                        contents_t = line_t_s[1].split(' ')
                                        ep_timing[current_len_index][4 * tx_ok + 2 + 0] = contents_t[2]
                                        ep_timing[current_len_index][4 * tx_ok + 2 + 1] = contents_t[3]
                                        ep_timing[current_len_index][4 * tx_ok + 2 + 2] = contents_t[4]
                                        ep_timing[current_len_index][4 * tx_ok + 2 + 3] = contents_t[5]
                                    line_t = f.readline()
                                    if len(line_t) > 1:
                                        line_t = line_t.replace('\n', '')
                                        line_t_s = line_t.split('} ')
                                        contents_t = line_t_s[1].split(' ')
                                        ep_timing[current_len_index][4 * tx_ok + 2 + 4 + 0] = contents_t[2]
                                        ep_timing[current_len_index][4 * tx_ok + 2 + 4 + 1] = contents_t[3]
                                        ep_timing[current_len_index][4 * tx_ok + 2 + 4 + 2] = contents_t[4]
    line = f.readline()
f.close()

for i in range(maximum_len - minimum_len + 1):
    print(str(i + minimum_len), '\t', end='')
    for j in range(required_cols[int(pkt_count) - 1] - 1):
        print(str(ep_timing[i][j]), '\t', end='')
    print(ep_timing[i][required_cols[int(pkt_count) - 1] - 1])
