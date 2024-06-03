#
# Create symbol alias for jiffies, sharing the value of jiffies_64
#
LD_OPT += --defsym=jiffies=jiffies_64
