byte bpe_flags[8192];

static inline void set_bpe(byte a,byte b)
{
  int ab=a;
  ab<<=5;
  ab+=b>>3;
  bpe_flags[ab]|=(1<<(b&7));
}

static inline void unset_bpe(byte a,byte b)
{
  int ab=a;
  ab<<=5;
  ab+=b>>3;
  bpe_flags[ab]&=~(1<<(b&7));
}

static inline int has_bpe(byte a,byte b)
{
  int ab=a;
  ab<<=5;
  ab+=b>>3;
  return bpe_flags[ab]&(1<<(b&7));
}

#define BPE 1024
int bpe_last_ofs[BPE];
int bpe_num;
int bpe_head;

void bpe_init() {
  bpe_num=0;
  bpe_head=0;
  memset(bpe_flags,0,8192);
}

void bpe_push(byte *buf, int pos)
{
  if (pos<2) return;
  byte a=buf[pos-2];
  byte b=buf[pos-1];
  if (has_bpe(a,b)) {
    return;
  }
  if (bpe_num==BPE) {
    int prev_pos=bpe_last_ofs[bpe_head];
    byte pa=buf[prev_pos];
    byte pb=buf[prev_pos+1];
    unset_bpe(pa,pb);
  }
  bpe_last_ofs[bpe_head++]=pos-2;
  if (bpe_head==BPE) bpe_head=0;
  if (bpe_num<BPE) bpe_num++;

  set_bpe(a,b);
}

int find_bpes(byte *buf, int n, int *offsets, int *rofs, int *totals)
{
  int bpe_index[256][256];
  int i;
  int cnt=0;

  bpe_init();
  offsets[0]=-1;
  rofs[0]=-1;
  totals[0]=0;
  offsets[1]=-1;
  rofs[1]=-1;
  totals[1]=1;
  if (buf[1]==buf[0]&&buf[2]==buf[1]) {
    offsets[1]=0;
    rofs[1]=0;
    totals[1]=1;
    cnt++;
  }
  for(i=2;i<n-1;i++) {
    int cur_head=bpe_head;
    bpe_push(buf,i);
    if (bpe_head!=cur_head)
      bpe_index[buf[i-2]][buf[i-1]]=cur_head;
    totals[i]=bpe_num+1;
    if (buf[i]==buf[i-1]&&buf[i]==buf[i+1]) {
      offsets[i]=i-1;
      rofs[i]=0;
      cnt++;
      continue;
    }
    if (has_bpe(buf[i],buf[i+1])) {
      int index=bpe_index[buf[i]][buf[i+1]];
      offsets[i]=bpe_last_ofs[index];
      index=bpe_head-index;
      if (index<0) index+=BPE;
      rofs[i]=index+1;
      cnt++;
      continue;
    }
    offsets[i]=-1;
    rofs[i]=-1;
  }
  return cnt;
}

int cnt_bpes(byte *buf, int n)
{
  int bpe_index[256][256];
  int i;
  int cnt=0;

  bpe_init();
  if (buf[1]==buf[0]&&buf[2]==buf[1]) {
    cnt++;
  }
  for(i=2;i<n-1;i++) {
    int cur_head=bpe_head;
    bpe_push(buf,i);
    if (bpe_head!=cur_head)
      bpe_index[buf[i-2]][buf[i-1]]=cur_head;
    if (buf[i]==buf[i-1]&&buf[i]==buf[i+1]) {
      cnt++;
      continue;
    }
    if (has_bpe(buf[i],buf[i+1])) {
      cnt++;
      continue;
    }
  }
  return cnt;
}
