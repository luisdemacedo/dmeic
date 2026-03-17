disable pretty-printer /home/Superficial/Mnemosyne/Aion/moco/mocoSource/Open-WBO-MO-base/open-wbo_debug open-wbo_debug;Model
shell truncate -s 0 clauses.txt
# clear Glucose::Solver::addClause(Glucose::vec<Glucose::Lit> const&)
# clear openwbo::UpperBoundHonerMO::searchBoundHonerMO 
set listsize 1
break Enc_KPA.cc:495
commands
  pipe printf "\ti is %d\n", i | cat >> clauses.txt
  pipe printf "\trhs is %d\n\n", rhs | cat >> clauses.txt
end

tbreak encoding::KPA::less_or_equal_than
commands
  silent
  enable $addclause
  up
  tbreak +1
  commands
    silent
    disable $addclause
    continue
  end
  down
  continue
end
break openwbo::UpperBoundHonerMO::searchBoundHonerMO
break Glucose::Solver::addClause(Glucose::vec<Glucose::Lit> const&)
set $addclause=$bpnum
disable $addclause
commands
  silent
  pipe print ps | perl -ne 'while(/(?<b>\{((?:[^\{\}]++ | (?&b))*+)\})/xg){$last= $2=~s/,//gr;};print "$last\n";'  >> clauses.txt
  select-frame function encoding::KPA::less_or_equal_than
  pipe list | cat >> clauses.txt
  advance +1
end

tbreak parseMOCNF
commands
  eval "shell cat %s >> clauses.txt", fname
  tbreak system
  commands
    call printf("rm disabled by gdb\n")
    call fflush(0)
    return
    continue
  end
  continue
end
run -cardinality=0 -pb=2 -no-bmo -mem-lim=8192 -cpu-lim=3600 -formula=1 -algorithm=16 -pbobjf=4 examples/debug.pbmo
