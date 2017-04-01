import asyncnet, asyncdispatch

const
  CRLF* = "\c\L"

proc processClient(address: string, client: AsyncSocket) {.async.} =
  echo "accepted connection from ", address
  while not client.isClosed():
    let line = await client.recvLine()
    if line == "":
      break
    await client.send(line & CRLF)
  echo address, " closed connection"

proc serve() {.async.} =
  let server = newAsyncSocket()
  server.bindAddr(7.Port)
  server.listen()

  echo "echo service listening on port 7"

  while true:
    let res = await server.acceptAddr()
    asyncCheck processClient(res[0], res[1])

asyncCheck serve()
runForever()
