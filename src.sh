submoduleList="./logic/uds_py ./logic/cobs ./logic"

sub_commit()
{
	for submodule in $submoduleList; do
		echo "Changing directory to $submodule"
		cd $submodule || exit
		echo "git add ."
		git add .
		echo "git commit -m $1"
		git commit -m "$1"
		cd - > /dev/null || exit
	done
	echo "git add ."
	git add .
	echo "git commit -m $1"
	git commit -m "$1"
}

sub_co()
{
	for submodule in $submoduleList; do
		echo "Changing directory to $submodule"
		cd $submodule || exit
		echo "git checkout $1"
		git checkout $1
		git pull
		cd - > /dev/null || exit
	done
	echo "git checkout $1"
	git checkout $1
}

sub_push()
{
	for submodule in $submoduleList; do
		echo "Changing directory to $submodule"
		cd $submodule || exit
		git push
		cd - > /dev/null || exit
	done
	git push
}

sub_st()
{
	for submodule in $submoduleList; do
		echo "Changing directory to $submodule"
		cd $submodule || exit
		echo "--------------------------------------------------------------"
		git status
		echo
		cd - > /dev/null || exit
	done
	echo "--------------------------------------------------------------"
	git status
}

sub_diff()
{
	for submodule in $submoduleList; do
		echo "Changing directory to $submodule"
		cd $submodule || exit
		echo "--------------------------------------------------------------"
		git diff
		echo
		cd - > /dev/null || exit
	done
	echo "--------------------------------------------------------------"
	git diff
}

sub_clear()
{
	for submodule in $submoduleList; do
		echo "Changing directory to $submodule"
		cd $submodule || exit
		git checkout -- .
		git clean -fdq
		cd - > /dev/null || exit
	done
	git checkout -- .
	git clean -fdq
}