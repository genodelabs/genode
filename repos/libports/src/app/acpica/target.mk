TARGET   := acpica
SRC_CC   := os.cc printf.cc report.cc
REQUIRES := x86
LIBS     += base acpica format

CC_CXX_WARN_STRICT =
