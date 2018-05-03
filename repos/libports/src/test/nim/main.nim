import unittest
const
  Text = "Hello world!"

suite "echo":
  echo Text

suite "system":
  echo "compile date: ", CompileDate, " - ", CompileTime
  echo("endianness: ",
    case cpuEndian:
    of littleEndian:
      "littleEndian"
    of bigEndian:
      "bigEndian"
  )
  echo "hostCPU: ", hostCPU
  echo "NimVersion: ", NimVersion
  echo "nativeStackTraceSupported: ", $nativeStackTraceSupported
  echo "getStackTrace:\n", getStackTrace()
  echo "getFreeMem: ", getFreeMem()
  echo "getTotalMem: ", getTotalMem()
  echo "getOccupiedMem: ", getOccupiedMem()

  test "isMainModule":
    assert isMainModule == true
  test "hostOS == genode":
    assert hostOS == "genode"

  test "alloc/dealloc":
    let p = alloc 768
    assert(not p.isNil)
    dealloc p

  test "exception handling":
    type NovelError = object of SystemError
    try: raise newException(NovelError, "test exception")
    except NovelError:
      discard

import locks, threadpool
suite "threadpool":

  test "spawn":
    var L: Lock

    proc threadProc(interval: tuple[a,b: int]) =
      for i in interval.a..interval.b:
        acquire L
        echo i
        release L

    initLock L
    for i in 0..3:
      spawn threadProc((i*10, i*10+4))
    sync()
    deinitLock L

  test "threadvar":
    var
      L: Lock
      x {.threadvar.}: int
      y: int

    proc printVal(id: string) =
      acquire L
      echo(
        id,
        " x: ", repr(addr(x)),
        " y: ", repr(addr(y))
      )
      inc x
      inc y
      release L

    initLock L
    release L
    printVal("main thread")
    for i in 1..4:
      spawn printVal("spawn "& $i)
    sync()
    deinitLock L

suite "I/O":
  const
    TestFile = "/testfile"
    Text = NimVersion & " - " & CompileDate & " - " & CompileTime
  test "writeFile":
    writeFile(TestFile, Text)
  test "readFile":
    assert readFile(TestFile) == Text

suite "staticExec":
  const rev = staticExec("git describe")
  echo "compile time 'git describe': ", rev

import times
suite "time":

  echo "epochTime() float value: ", epochTime()
  echo "getTime()   float value: ", toSeconds(getTime())
  echo "cpuTime()   float value: ", cpuTime()
  echo "An hour from now      : ", getLocalTime(getTime()) + 1.hours
  echo "An hour from (UTC) now: ", getGmTime(getTime()) + initInterval(0,0,0,1)

suite "garbage collector":
  echo GC_getStatistics()

echo "done"
quit 0
