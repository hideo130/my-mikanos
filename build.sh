#! /bin/zsh -eu

echo $BASEDIR
make ${MAKE_OPTS:-} -C $MYMIKANOS/kernel kernel.elf

for MK in $(ls $MYMIKANOS/apps/*/Makefile)
do
    echo "MK $MK"
    APP_DIR=$(dirname $MK)
    APP=$(basename $APP_DIR)
    make ${MAKE_OPTS:-} -C $APP_DIR $APP
done

if [ "${1:-}" = "run" ]
then
    $OS_SHELL_DIR/run_qemu.sh $EDK2DIR/Build/MyMikanLoaderX64/DEBUG_CLANGPDB/X64/Loader.efi $MYMIKANOS/kernel/kernel.elf
fi
