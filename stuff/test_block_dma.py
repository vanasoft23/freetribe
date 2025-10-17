
def previous_power_of_two(x: int) -> int:
    x |= (x >> 1)
    x |= (x >> 2)
    x |= (x >> 4)
    x |= (x >> 8)
    x |= (x >> 16)
    return x - (x >> 1)


# def ceil2(v):
#     v -= 1
#     v |= v >> 1
#     v |= v >> 2
#     v |= v >> 4
#     v |= v >> 8
#     v |= v >> 16
#     v += 1
    # return v


# for i in range(17):
#     print(i, previous_power_of_two(i))

words_remaining = 55

while (words_remaining > 0):

    block_length = min(words_remaining, 16)
    block_length_2 = previous_power_of_two(block_length)
    print(block_length, block_length_2, words_remaining)
    words_remaining -= block_length_2;