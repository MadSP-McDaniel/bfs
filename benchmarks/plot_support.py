def lookup_size(_s):
    s = int(_s)
    # print(s)
    if s == 0:
        return "1G"
    if s == 1:
        return "2G"
    if s == 2:
        return "4G"
    if s == 3:
        return "8G"
    if s == 4:
        return "1T"
    if s == 5:
        return "2T"
    if s == 6:
        return "4T"
    if s == 7:
        return "8T"
    if s == 8:
        return "16T"
    # if s == 2**20:
    #     return "1M"
    # if s == 2**25:
    #     return "32M"
    # if s == 2**27:
    #     return "128M"
    # if s == 2**29:
    #     return "512M"
    # if s == 2**30:
    #     return "1G"
    # if s == 2**33:
    #     return "8G"
    # if s == 2**35:
    #     return "32G"
    # if s == 2**36:
    #     return "64G"
    # if s == 2**37:
    #     return "128G"
    # if s == 2**38:
    #     return "256G"
    # if s == 2**39:
    #     return "512G"
    # if s == 2**40:
    #     return "1T"
    # if s == 2**41:
    #     return "2T"
    # if s == 2**42:
    #     return "4T"
    # if s == 2**43:
    #     return "8T"
    # if s == 2**44:
    #     return "16T"
    # if s == 2**45:
    #     return "32T"
    # if s == 2**46:
    #     return "64T"

    return "XB"


# def lookup_mt_type(t):
#     if t == "-1":
#         return "Encryption/no integrity"
#     if t == "0":
#         return "dm-verity (SoTA)"
#     if t == "3":
#         return "H-OPT"
#     if t == "4":
#         return "DMT"
#     if t == "5":
#         return "No encryption/no integrity"
#     if t == "6":
#         return "SoTA [46] (64-ary)"
def lookup_mt_type(t):
    return t
