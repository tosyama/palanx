#!/bin/sh
# Removes the Palan version this script belongs to.
# Everything runs inside a function because the script deletes itself: the shell
# has then read all of it, and never reads the removed file again.

main() {
	self=$(readlink -f "$0")
	version_dir=$(dirname "$self")
	version=$(basename "$version_dir")
	palan_dir=$(dirname "$version_dir")
	lib_dir=$(dirname "$palan_dir")
	root=$(dirname "$lib_dir")

	if [ "$(basename "$palan_dir")" != palan ] || [ "$(basename "$lib_dir")" != lib ]; then
		echo "uninstall: $self is not in <root>/lib/palan/<version>/" >&2
		exit 1
	fi

	link="$root/bin/palan"
	link_target=
	if [ -L "$link" ]; then
		link_target=$(readlink "$link")
	fi

	rm -rf "$version_dir" "$root/share/doc/palan/$version"

	if [ "$link_target" = "../lib/palan/$version/tools/palan" ]; then
		newest=$(ls "$palan_dir" | sort -V | tail -n 1)
		if [ -n "$newest" ]; then
			ln -sfn "../lib/palan/$newest/tools/palan" "$link"
		else
			rm -f "$link"
		fi
	fi

	for dir in "$palan_dir" "$lib_dir" "$root/share/doc/palan" "$root/share/doc" "$root/share" "$root/bin"; do
		rmdir "$dir" 2>/dev/null
	done
	exit 0
}

main
