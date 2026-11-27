# a stand-in radio station: /stream is an endless ICY MP3 stream (the file looped); /list.pls points at it; /moved redirects
import sys, socket, threading
port, mp3 = int(sys.argv[1]), open(sys.argv[2], 'rb').read()
def serve(c):
    try:
        req = c.recv(4096).decode('latin1'); path = req.split(' ')[1] if ' ' in req else '/'
        if path == '/list.pls': body = ('[playlist]\nNumberOfEntries=1\nFile1=http://127.0.0.1:%d/stream\n' % port).encode(); c.sendall(b'HTTP/1.0 200 OK\r\nContent-Type: audio/x-scpls\r\n\r\n' + body); return
        if path == '/moved': c.sendall(('HTTP/1.1 302 Found\r\nLocation: http://127.0.0.1:%d/stream\r\n\r\n' % port).encode()); return
        if path != '/stream': c.sendall(b'HTTP/1.0 404 Not Found\r\n\r\n'); return
        c.sendall(b'ICY 200 OK\r\nicy-name: test\r\nContent-Type: audio/mpeg\r\n\r\n')
        while True: c.sendall(mp3)
    except Exception: pass
    finally: c.close()
s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(('127.0.0.1', port)); s.listen(8)
while True:
    c, _ = s.accept(); threading.Thread(target=serve, args=(c,), daemon=True).start()
