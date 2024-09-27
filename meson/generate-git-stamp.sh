#! /bin/sh

git_dir=$1
stamp_file=$2

if test -r $git_dir; then
    stamp=`git --git-dir=$git_dir rev-list --abbrev-commit -1 HEAD`
    if test \! -s $stamp_file || grep -wvq $stamp $stamp_file; then
	date=`git --git-dir=$git_dir log -1 $stamp --pretty=format:%ci`
	echo "#define GIT_STAMP \"#$stamp ($date)\"" > $stamp_file
    fi
elif test \! -r $stamp_file -o -s $stamp_file; then
    rm -f $stamp_file && touch $stamp_file
fi

exit 0
