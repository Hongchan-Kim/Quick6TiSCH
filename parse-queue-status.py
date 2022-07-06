import argparse

parser = argparse.ArgumentParser()
parser.add_argument('any_iter')
parser.add_argument('any_id')
args = parser.parse_args()

any_iter = args.any_iter
any_id = args.any_id

ucsf_len = 17

queue_all_name = 'queue-all-' + any_iter + '-' + any_id + '.txt'
queue_all = open(queue_all_name, 'w')
queue_all.write('asn' + '\t' + 'suc' + '\t' + 'glb' + '\t' + 'uc' + '\t' + 'bc' + '\t' + 'eb' + '\n')

queue_add_name = 'queue-add-' + any_iter + '-' + any_id + '.txt'
queue_add = open(queue_add_name, 'w')
queue_add.write('asn' + '\t' + 'suc' + '\t' + 'glb' + '\t' + 'uc' + '\t' + 'bc' + '\t' + 'eb' + '\n')

queue_free_name = 'queue-free-' + any_iter + '-' + any_id + '.txt'
queue_free = open(queue_free_name, 'w')
queue_free.write('asn' + '\t' + 'suc' + '\t' + 'glb' + '\t' + 'uc' + '\t' + 'bc' + '\t' + 'eb' + '\n')

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
                if contents[0] == 'QU':
                    if contents[3] == '1': # unicast packet
                        current_asn = int(contents[5], 16)
                        if contents[1] == 'as':
                            queue_add.write(str(current_asn) + '\t' + 's' + '\t' + contents[7] + '\t' + contents[8] + '\t' + contents[9] + '\t' + contents[10] + '\n')
                            queue_all.write(str(current_asn) + '\t' + 's' + '\t' + contents[7] + '\t' + contents[8] + '\t' + contents[9] + '\t' + contents[10] + '\n')
                        elif contents[1] == 'af1':
                            queue_add.write(str(current_asn) + '\t' + 'f1' + '\t' + contents[7] + '\t' + contents[8] + '\t' + contents[9] + '\t' + contents[10] + '\n')
                            queue_all.write(str(current_asn) + '\t' + 'f1' + '\t' + contents[7] + '\t' + contents[8] + '\t' + contents[9] + '\t' + contents[10] + '\n')
                        elif contents[1] == 'af2':
                            queue_add.write(str(current_asn) + '\t' + 'f2' + '\t' + contents[7] + '\t' + contents[8] + '\t' + contents[9] + '\t' + contents[10] + '\n')
                            queue_all.write(str(current_asn) + '\t' + 'f2' + '\t' + contents[7] + '\t' + contents[8] + '\t' + contents[9] + '\t' + contents[10] + '\n')
                        elif contents[1] == 'f':
                            queue_free.write(str(current_asn) + '\t' + 'f' + '\t' + contents[7] + '\t' + contents[8] + '\t' + contents[9] + '\t' + contents[10] + '\n')
                            queue_all.write(str(current_asn) + '\t' + 'f' + '\t' + contents[7] + '\t' + contents[8] + '\t' + contents[9] + '\t' + contents[10] + '\n')
    line = f.readline()
f.close()

queue_all.close()
queue_add.close()
queue_free.close()

queue_add_sum_name = 'queue-add-summary-' + any_iter + '-' + any_id + '.txt'
queue_add_sum = open(queue_add_sum_name, 'w')
queue_add_sum.write('asfn' + '\t' + 'add' + '\n')

current_asfn = -1
current_asfn_add = 0
print_asfn = -1

f = open(queue_add_name, 'r', errors='ignore')
line = f.readline()
line = f.readline()
while line:
    if len(line) > 1:
        line = line.replace('\n','')
        line_s = line.split('\t')

        line_asfn = int(line_s[0]) // ucsf_len
        if current_asfn < line_asfn:
            if current_asfn > -1:
                queue_add_sum.write(str(current_asfn) + '\t' + str(current_asfn_add) + '\n')
                print_asfn = current_asfn + 1
                while print_asfn < line_asfn:
                    queue_add_sum.write(str(print_asfn) + '\t' + str(0) + '\n')
                    print_asfn += 1
            current_asfn = line_asfn
            current_asfn_add = 1
        elif current_asfn == line_asfn:
            current_asfn_add += 1
    line = f.readline()
f.close()
queue_add_sum.close()


queue_free_sum_name = 'queue-free-summary-' + any_iter + '-' + any_id + '.txt'
queue_free_sum = open(queue_free_sum_name, 'w')
queue_free_sum.write('asfn' + '\t' + 'free' + '\n')

current_asfn = -1
current_asfn_free = 0
print_asfn = -1

f = open(queue_free_name, 'r', errors='ignore')
line = f.readline()
line = f.readline()
while line:
    if len(line) > 1:
        line = line.replace('\n','')
        line_s = line.split('\t')

        line_asfn = int(line_s[0]) // ucsf_len
        if current_asfn < line_asfn:
            if current_asfn > -1:
                queue_free_sum.write(str(current_asfn) + '\t' + str(current_asfn_free) + '\n')
                print_asfn = current_asfn + 1
                while print_asfn < line_asfn:
                    queue_free_sum.write(str(print_asfn) + '\t' + str(0) + '\n')
                    print_asfn += 1
            current_asfn = line_asfn
            current_asfn_free = 1
        elif current_asfn == line_asfn:
            current_asfn_free += 1
    line = f.readline()
f.close()
queue_free_sum.close()
