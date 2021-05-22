import gzip
import shutil
import os, glob

FIRMWARE_PACKING_ENABLED = True


#
# Code is copied from:
# https://gist.github.com/andrewwalters/d4e3539319e55fc980db1ba67254d7ed
#
def binary_compress(target_file, source_file):
    """ Compress ESP8266 firmware using gzip for 'compressed OTA upload' """
    do_compress = True
    source_file_bak = source_file
    if target_file == source_file:
        source_file_bak = source_file + ".bak"

    if os.path.exists(target_file) and os.path.exists(source_file_bak):
        """ Recompress if source file is newer than target file """
        tgt_mtime = os.stat(target_file).st_mtime
        src_mtime = os.stat(source_file_bak).st_mtime
        if target_file == source_file:
            """ bak file is older than source file """
            do_compress = (src_mtime < tgt_mtime)
        else:
            """ target file is older than source file """
            do_compress = (src_mtime > tgt_mtime)

    if do_compress:
        print("Compressing firmware for upload...")
        if target_file == source_file:
            shutil.move(source_file, source_file_bak)
        with open(source_file_bak, 'rb') as f_in:
            with gzip.open(target_file, 'wb') as f_out:
                shutil.copyfileobj(f_in, f_out)
        """ Set modification time on compressed file so incremental build works """
        shutil.copystat(source_file_bak, target_file)
        """ print compression info """
        size_orig = os.stat(source_file_bak).st_size
        size_gz = os.stat(target_file).st_size
        print("Compression reduced firmware size to {:.0f}% (was {} bytes, now {} bytes)".format(
            (size_gz / size_orig) * 100, size_orig, size_gz))


def compressFirmware(source, target, env):
    """ Compress ESP8266 firmware using gzip for 'compressed OTA upload' """
    if FIRMWARE_PACKING_ENABLED:
        build_dir = env.subst("$BUILD_DIR")
        image_name = env.subst("$PROGNAME")
        source_file = os.path.join(build_dir, image_name + ".bin")
        target_file = source_file + ".gz"
        binary_compress(target_file, source_file)


def compress_files(source, target, env):
    #add filetypes (extensions only) to be gzipped before uploading. Everything else will be copied directly
    filetypes_to_gzip = ['js', 'html', 'css']
    #filetypes_to_gzip = []

    project_dir = env.get('PROJECT_DIR')
    platform = env.get('PIOPLATFORM', '')
    if platform == 'espressif8266':
        data_src_dir = os.path.join(project_dir,
            "utils", "ESP8266SerialToWebsocket", "html")
    else:
        data_src_dir = os.path.join(project_dir,
            "src", "esp32", "html")

    print(' ***** Packing html files *****')

    data_dir = env.get('PROJECTDATA_DIR')

    if not os.path.exists(data_src_dir):
        print('    HTML source "%s" does not found.' % data_src_dir)
        return

    if os.path.exists(data_dir):
        print('    RM data dir: ' + data_dir)
        shutil.rmtree(data_dir)

    print('    MK data dir: ' + data_dir)
    os.mkdir(data_dir)

    files_to_gzip = []
    for extension in filetypes_to_gzip:
        files_to_gzip.extend(glob.glob(os.path.join(data_src_dir, '*.' + extension)))

    #print('    => files to gzip: ' + str(files_to_gzip))

    all_files = glob.glob(os.path.join(data_src_dir, '*.*'))
    files_to_copy = list(set(all_files) - set(files_to_gzip))

    for f_name in files_to_copy:
        print('    Copy: ' + os.path.basename(f_name))
        shutil.copy(f_name, data_dir)

    for f_name in files_to_gzip:
        print('    GZip: ' + os.path.basename(f_name))
        with open(f_name, 'rb') as f_in:
            out_file = os.path.join(data_dir, os.path.basename(f_name) + '.gz')
            with gzip.open(out_file, 'wb') as f_out:
                shutil.copyfileobj(f_in, f_out)

    print('   packing done!')

def compress_fs_bin(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    image_name = env.get('ESP8266_FS_IMAGE_NAME')
    src_file = os.path.join(build_dir, image_name + ".bin")
    target_file = src_file + ".gz"
    binary_compress(target_file, src_file)
