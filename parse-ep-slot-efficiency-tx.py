import argparse

parser = argparse.ArgumentParser()
parser.add_argument('any_iter')
parser.add_argument('any_id')
args = parser.parse_args()

any_iter = args.any_iter
any_id = args.any_id

minimum_len = 46
maximum_len = 125
packet_count = 14
num_of_slots = [[0 for col in range(packet_count)] for row in range(maximum_len - minimum_len + 1)]
updated = [[0 for col in range(packet_count)] for row in range(maximum_len - minimum_len + 1)]


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
                if contents[0] == 'ep' and contents[1] == 'result' and contents[2] == 'ucsf' and contents[3] == 'tx':
                    current_len = int(contents[4])
                    if current_len >= minimum_len:
                        current_len_index = current_len - minimum_len
                        all_same_len = int(contents[5])
                        tx_count = int(contents[6])
                        tx_ok = int(contents[7])
                        if all_same_len == 1 and tx_count == tx_ok:
                            current_len_index = current_len - minimum_len
                            if updated[current_len_index][tx_count - 1] == 0:
                                updated[current_len_index][tx_count - 1] = 1
                                num_of_slots[current_len_index][tx_count - 1] = int(contents[8])
    line = f.readline()
f.close()

for i in range(maximum_len - minimum_len + 1):
    print(str(i + minimum_len), '\t', end='')
    for j in range(packet_count - 1):
        print(str(num_of_slots[i][j]), '\t', end='')
    print(num_of_slots[i][packet_count - 1])
