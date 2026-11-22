# usage: cap.sh <label> <file.cpp> -> runs syntax-only with unused-capture/shadow warnings, prints warning lines
L=$1; F=$2
R=/home/user/precheck/out/$L/logs/$F.rsp
O=/home/user/runs/cap/$L-$F.rsp
sed -e 's/^-Wno-unused-lambda-capture$/-Wunused-lambda-capture -Wno-error=unused-lambda-capture -Wshadow-uncaptured-local -Wno-error=shadow-uncaptured-local/' \
    -e '/^-c$/d' -e '/^-O0$/d' -e '/^-o /d' "$R" > $O
echo -fsyntax-only >> $O
nice -n 10 clang++-20 @$O > /home/user/runs/cap/$L-$F.txt 2>&1
echo "$L $F rc $? warnings $(grep -c 'warning:' /home/user/runs/cap/$L-$F.txt)"
