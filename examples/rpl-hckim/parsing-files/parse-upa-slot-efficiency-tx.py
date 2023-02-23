import argparse

parser = argparse.ArgumentParser()
parser.add_argument('any_scheduler')
parser.add_argument('any_iter')
parser.add_argument('any_id')
args = parser.parse_args()

any_scheduler = args.any_scheduler
any_iter = args.any_iter
any_id = args.any_id

minimum_len = 59
maximum_len = 125
packet_count = 16
num_of_slots = [[0 for col in range(packet_count)] for row in range(maximum_len - minimum_len + 1)]
updated = [[0 for col in range(packet_count)] for row in range(maximum_len - minimum_len + 1)]
boundaries = [[0 for col in range(packet_count)] for row in range(maximum_len - minimum_len + 1)]
num_of_expected_slots = [[0 for col in range(packet_count)] for row in range(maximum_len - minimum_len + 1)]

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
                if contents[0] == 'upa' and contents[1] == 'result' and (contents[2] == 'ucsf' or contents[2] == 'bcsf') and contents[3] == 'tx':
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
                                num_of_expected_slots[current_len_index][tx_count - 1] = int(contents[10])
                            else:
                                if num_of_slots[current_len_index][tx_count - 1] < int(contents[8]):
                                    boundaries[current_len_index][tx_count - 1] = 1
                                    num_of_slots[current_len_index][tx_count - 1] = int(contents[8])
                                    num_of_expected_slots[current_len_index][tx_count - 1] = int(contents[10])
    line = f.readline()
f.close()

print("updated")
for i in range(maximum_len - minimum_len + 1):
    print(str(i + minimum_len), '\t', end='')
    for j in range(packet_count - 1):
        print(str(updated[i][j]), '\t', end='')
    print(updated[i][packet_count - 1])
print()

print("boundaries")
for i in range(maximum_len - minimum_len + 1):
    print(str(i + minimum_len), '\t', end='')
    for j in range(packet_count - 1):
        print(str(boundaries[i][j]), '\t', end='')
    print(boundaries[i][packet_count - 1])
print()

print("num of expected slots")
for i in range(maximum_len - minimum_len + 1):
    print(str(i + minimum_len), '\t', end='')
    for j in range(packet_count - 1):
        print(str(num_of_expected_slots[i][j]), '\t', end='')
    print(num_of_slots[i][packet_count - 1])
print()

print("num of slots")
for i in range(maximum_len - minimum_len + 1):
    print(str(i + minimum_len), '\t', end='')
    for j in range(packet_count - 1):
        print(str(num_of_slots[i][j]), '\t', end='')
    print(num_of_slots[i][packet_count - 1])
print()

print("slot utility")
for i in range(maximum_len - minimum_len + 1):
    print(str(i + minimum_len), '\t', end='')
    print(str(round(1,2)),'\t',end='')
    for j in range(packet_count - 1):
        if updated[i][j] == 1:
            print(str(round((j + 2)/(num_of_slots[i][j] + 1),2)), '\t', end='')
        else:
            print("N/A", '\t', end='')
    if updated[i][packet_count - 1] == 1:
        print(str(round((packet_count + 1)/(num_of_slots[i][packet_count - 1] + 1),2)))
    else:
        print("N/A")
print()

