
def get_table(file_name):

    res = {}

    with open(file_name) as f:
        elements = f.readlines()
        for el in elements:
            values = el.strip().split()
            res[values[0]] = int(values[2])
    
    return res


def get_perft_table(file_name):
    
    res = {}

    with open(file_name) as f:
        elements = f.readlines()
        for el in elements:
            values = el.strip().split()
            res[values[1]] = int(values[4])
    
    return res


def main():

    list1 = get_table("inp.txt")
    list2 = get_table("inp2.txt")
    # list2 = get_perft_table("inp2.txt")

    res = {}

    for key, value in list1.items():
        if key in list2:
            res[key] = value - list2[key]
        else:
            print(f'Key {key} not found in list2')
            return
    
    for key, value in res.items():
        print(f'{key} --> {value}')




if __name__ == '__main__':
    main()
