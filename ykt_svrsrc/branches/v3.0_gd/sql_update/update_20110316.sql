-- �̻����ѷֳ����ñ�
create table T_SHOPBOARDFEE
(
  CRACCNO  VARCHAR2(21) not null,
  DRACCNO  VARCHAR2(21) not null,
  RATE     NUMBER(4,2) not null,
  OPERCODE VARCHAR2(8),
  OPERDATE VARCHAR2(8),
  OPERTIME VARCHAR2(6),
  PRIMARY KEY (CRACCNO,DRACCNO)
);

insert into t_cfgsplit (TRANSTYPE, FUNDTYPE, OFFLINEFLAG, USECARDFLAG, CALCCARDBAL, DRACCFLAG, DRSUBJNO, DRACCNO, CRACCFLAG, CRSUBJNO, CRACCNO, SUMMARY)
values (901, 7, 0, 0, null, 'I', '0', '', 'I', '', '0', '���ѷֳ�');