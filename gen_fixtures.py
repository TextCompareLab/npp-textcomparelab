import random,os
random.seed(7)
os.makedirs("fx",exist_ok=True)
heb=["אלון","שלום עולם","בדיקה","שורה אחת","ğüş çalışma","日本語のテキスト","emoji 😀 line"]
def mk(n,eol,final):
    L=[random.choice(heb+["line %d"%i,"cpu 8:00 - 17:00","  indented","",""]) for i in range(n)]
    s=eol.join(L)
    return s+(eol if final else "")
cases=[("insert_delete","a\nb\nc\nd\ne\nf\ng\nh\ni\nj\n","a\nb\nX\nd\ne\nf\ng\nh\nj\nk\n"),
("hebrew","אלון\nשלום\nעולם\n","אלון\nשלום!\nעולם\nחדש\n"),
("no_final_nl_a","x\ny\nz","x\ny\nz\n"),("no_final_nl_b","x\ny\nz\n","x\ny\nz"),("both_no_nl","x\ny\nz","x\nY\nz"),
("crlf","a\r\nb\r\nc\r\n","a\r\nB\r\nc\r\nd\r\n"),("crlf_to_lf","a\r\nb\r\nc\r\n","a\nb\nc\n"),
("mixed_eol","a\r\nb\nc\r\nd\n","a\r\nb\nC\r\nd\n"),("empty_to_text","","hello\nworld\n"),("text_to_empty","hello\nworld\n",""),
("whitespace_only","a  b\nc\n","a b\nc\n"),("start_insert","b\nc\n","a\nb\nc\n"),("end_delete","a\nb\nc\n","a\nb\n")]
for k in range(25):
    eol=random.choice(["\n","\r\n"]); n=random.randint(1,60)
    A=mk(n,eol,random.random()<.7).split(eol); B=A[:]
    for _ in range(random.randint(1,8)):
        op=random.choice("idc")
        if not B: break
        p=random.randrange(len(B))
        if op=="i": B.insert(p,random.choice(heb+["new"]))
        elif op=="d": B.pop(p)
        else: B[p]=B[p]+" edit"
    cases.append(("rand%d"%k,eol.join(A),eol.join(B) if random.random()<.8 else eol.join(B)+eol))
for name,a,b in cases:
    open("fx/%s.a"%name,"wb").write(a.encode()); open("fx/%s.b"%name,"wb").write(b.encode())
