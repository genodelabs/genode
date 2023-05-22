#!/bin/bash

echo "--- Automated Tresor testing ---"

produce_pattern() {
	local pattern="$1"
	local size="$2"
	[ "$pattern" = "" ] && exit 1

	local tmp_file="/tmp/pattern.tmp"
	local N=1041
	# prints numbers until N and uses pattern as delimiter and
	# generates about 4 KiB of data with a 1 byte pattern
	seq -s "$pattern" $N > $tmp_file
	dd if=$tmp_file count=1 bs=$size 2>/dev/null
}

test_write_1() {
	local data_file="$1"
	local offset=$2

	local pattern_file="/tmp/pattern"
	dd bs=4096 count=1 if=$pattern_file of=$data_file seek=$offset 2>/dev/null || exit 1
}

test_read_seq_unaligned_512() {
	local data_file="$1"
	local length="$2"
	dd bs=512 count=$((length / 512)) if=$data_file of=/dev/null
}

test_read_compare_1() {
	local data_file="$1"
	local offset=$2

	local pattern_file="/tmp/pattern"
	rm $pattern_file.out 2>/dev/null

	dd bs=4096 count=1 if=$data_file of=$pattern_file.out skip=$offset 2>/dev/null || exit 1
	local sha1=$(sha1sum $pattern_file)
	local sha1_sum=${sha1:0:40}

	local sha1out=$(sha1sum $pattern_file.out)
	local sha1out_sum=${sha1out:0:40}

	if [ "$sha1_sum" != "$sha1out_sum" ]; then
		echo "mismatch for block $offset:"
		echo "  expected: $sha1_sum"
		echo -n "  "
		dd if=$pattern_file bs=32 count=1 2>/dev/null; echo
		echo "  got: $sha1out_sum"
		echo -n "  "
		dd if=$pattern_file.out bs=32 count=1 2>/dev/null; echo
		return 1
	fi
}

test_create_snapshot() {
	local tresor_dir="$1"

	echo "Create snapshot"
	echo true > $tresor_dir/control/create_snapshot
}

test_list_snapshots() {
	local tresor_dir="$1"

	echo "List content of '$tresor_dir'"
	ls -l $tresor_dir/snapshots
}

test_discard_snapshot() {
	local tresor_dir="$1"
	local snap_id=$2

	echo "Discard snapshot with id: $snap_id"
	echo $snap_id > $tresor_dir/control/discard_snapshot
}

test_rekey_start() {
	local tresor_dir="$1"

	echo "Start rekeying"
	echo on > $tresor_dir/control/rekey
	echo "Reykeying started"
}

test_vbd_extension() {
	local tresor_dir="$1"
	local nr_of_phys_blocks="$2"

	echo "Start extending VBD"
	echo tree=vbd, blocks=$nr_of_phys_blocks > $tresor_dir/control/extend
	echo "VBD extension started"
}

test_ft_extension() {
	local tresor_dir="$1"
	local nr_of_phys_blocks="$2"

	echo "Start extending FT"
	echo tree=ft, blocks=$nr_of_phys_blocks > $tresor_dir/control/extend
	echo "FT extension started"
}

test_rekey_state() {
	local tresor_dir="$1"
	local state="$(< $tresor_dir/control/rekey)"
	local progress="$(< $tresor_dir/control/rekey_progress)"

	local result="unknown"
	case "$progress" in
	*at*)
		result="$progress"
		;;
	*idle*)
		result="done"
		;;
	esac

	echo "Rekeying state: $state progress: $result"
}

test_rekey() {
	local tresor_dir="$1"

	test_rekey_start "$tresor_dir"

	wait_for_rekeying "$tresor_dir" "yes"
}

wait_for_rekeying() {
	local tresor_dir="$1"
	local verbose="$2"
	local result="unknown"

	echo "Wait for rekeying to finish..."
	while : ; do
		local done=0
		local file_content="$(< $tresor_dir/control/rekey_progress)"
		# XXX remove later
		echo "file_content: ${file_content}"
		case "$file_content" in
		*at*)
			if [ "$verbose" = "yes" ]; then
				echo "Rekeying: $file_content"
			fi
			;;
		*idle*)
			done=1;
			;;
		esac
		if [ $done -gt 0 ]; then
			break
		fi
	done
	echo "Rekeying done"
}

wait_for_vbd_extension() {
	local tresor_dir="$1"

	echo "Wait for VBD extension to finish..."
	while : ; do
		local done=0
		local file_content="$(< $tresor_dir/control/extend_progress)"
		# XXX remove later
		echo "file_content: ${file_content}"
		case "$file_content" in
		*at*)
			if [ "$verbose" = "yes" ]; then
				echo "Extending VBD: $file_content"
			fi
			;;
		*idle*)
			done=1;
			;;
		esac
		if [ $done -gt 0 ]; then
			break
		fi
	done
	echo "VBD extension done"
}

wait_for_ft_extension() {
	local tresor_dir="$1"

	echo "Wait for FT extension to finish..."
	while : ; do
		local done=0
		local file_content="$(< $tresor_dir/control/extend_progress)"
		# XXX remove later
		echo "file_content: ${file_content}"
		case "$file_content" in
		*at*)
			if [ "$verbose" = "yes" ]; then
				echo "Extending FT: $file_content"
			fi
			;;
		*idle*)
			done=1;
			;;
		esac
		if [ $done -gt 0 ]; then
			break
		fi
	done
	echo "FT extension done"
}

main() {
	local tresor_dir="/dev/tresor"
	local data_file="$tresor_dir/current/data"

	ls -l $tresor_dir

	for i in $(seq 3); do

		echo "--> Run $i:"

		test_read_seq_unaligned_512 "$data_file" "1048576"

		local pattern_file="/tmp/pattern"
		produce_pattern "$i" "4096" > $pattern_file

		test_write_1 "$data_file" "419"
		test_write_1 "$data_file"  "63"
		test_write_1 "$data_file" "333"

		test_vbd_extension "$tresor_dir" "1000"
		test_read_compare_1 "$data_file" "63"
		test_write_1 "$data_file" "175"
		test_read_compare_1 "$data_file" "419"
		test_write_1 "$data_file" "91"
		test_read_compare_1 "$data_file" "175"
		test_read_compare_1 "$data_file" "91"
		test_read_compare_1 "$data_file" "333"
		wait_for_vbd_extension "$tresor_dir"

		test_write_1 "$data_file"  "32"
		test_write_1 "$data_file"  "77"
		test_write_1 "$data_file" "199"

		#test_ft_extension "$tresor_dir" "1000"
		test_read_compare_1 "$data_file" "32"
		test_write_1 "$data_file" "211"
		test_read_compare_1 "$data_file" "77"
		test_write_1 "$data_file" "278"
		test_read_compare_1 "$data_file" "199"
		test_read_compare_1 "$data_file" "278"
		test_read_compare_1 "$data_file" "211"
		#wait_for_ft_extension "$tresor_dir"

		test_write_1 "$data_file"  "0"
		test_write_1 "$data_file"  "8"
		test_write_1 "$data_file" "16"
		test_write_1 "$data_file" "490"
		test_write_1 "$data_file" "468"

		test_read_compare_1 "$data_file" "0"
		test_read_compare_1 "$data_file" "8"
		test_read_compare_1 "$data_file" "16"
		test_read_compare_1 "$data_file" "490"

		#test_rekey "$tresor_dir"

		test_rekey_start "$tresor_dir"
		test_write_1 "$data_file" "0"
		test_rekey_state "$tresor_dir"
		test_read_compare_1 "$data_file" "490"
		test_rekey_state "$tresor_dir"
		test_write_1 "$data_file" "16"
		test_rekey_state "$tresor_dir"
		test_read_compare_1 "$data_file" "468"
		test_rekey_state "$tresor_dir"
		test_read_compare_1 "$data_file" "8"
		test_rekey_state "$tresor_dir"
		test_read_compare_1 "$data_file" "16"
		test_rekey_state "$tresor_dir"
		test_read_compare_1 "$data_file" "0"
		test_write_1 "$data_file" "300"
		test_write_1 "$data_file" "240"
		test_write_1 "$data_file" "201"
		test_write_1 "$data_file" "328"
		wait_for_rekeying "$tresor_dir" "yes"

		echo "--> Run $i done"

	done

	echo "--> Read/Compare test"
	test_read_compare_1 "$data_file" "0"
	test_read_compare_1 "$data_file" "490"
	test_read_compare_1 "$data_file" "468"
	test_read_compare_1 "$data_file" "8"
	test_read_compare_1 "$data_file" "16"
	echo "--> Read/Compare test done"

	echo "--- Automated Tresor testing finished, shell is yours ---"
}

main "$@"

# just drop into shell
# exit 0
