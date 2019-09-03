########################################################################
#                                                                      #
#               This software is part of the ast package               #
#          Copyright (c) 1982-2012 AT&T Intellectual Property          #
#                      and is licensed under the                       #
#                 Eclipse Public License, Version 1.0                  #
#                    by AT&T Intellectual Property                     #
#                                                                      #
#                A copy of the License is available at                 #
#          http://www.eclipse.org/org/documents/epl-v10.html           #
#         (with md5 checksum b35adb5213ca9657e911e9befb180842)         #
#                                                                      #
#              Information and Software Systems Research               #
#                            AT&T Research                             #
#                           Florham Park NJ                            #
#                                                                      #
#                    David Korn <dgkorn@gmail.com>                     #
#                                                                      #
########################################################################

tmp1=$TEST_DIR/tmp1.csv
tmp2=$TEST_DIR/tmp2.csv
cat > $tmp1 <<- \EOF
	CAT,"CVE CCODE","NECA OCN",ST,LATA,AP,"New InterState
	Orig","New Inter""""State
	Term","New IntraState
	Orig","New IntraState
	Term"
	CLEC,XXXX,AAAA,RB,ABC,comp," 0.2 "," 0.4 "," 0.6 "," 0.8 "
	CLEC,YYYY,QQQQ,SX,123,mmmm," 0.3 "," 0.5 "," 0.7 "," 0.9 "
EOF

integer count=0 nfields
IFS=${2-,}
for j in 1 2
do
    typeset -a arr
    while read -A -S arr
    do
        ((nfields=${#arr[@]}))
        if ((++count==1))
        then
            ((nfields==10)) || log_error 'first record should contain 10 fields'
            [[ ${arr[7]} == $'New Inter""State\nTerm' ]] || log_error $'7th field of record 1 should contain New Inter""State\nTerm'
        fi

        for ((i=0; i < nfields;i++))
        do
            delim=$IFS
            if ((i == nfields-1))
            then
                delim=$'\r\n'
            fi

            if ((i==1))
            then
                printf "%#q%s"  "${arr[i]}" "$delim"
            else
                printf "%(csv)q%s"  "${arr[i]}" "$delim"
            fi
        done
    done < $tmp1 > $tmp2
done
diff "$tmp1" "$tmp2" >/dev/null 2>&1 || log_error "files $tmp1 and $tmp2 differ"
