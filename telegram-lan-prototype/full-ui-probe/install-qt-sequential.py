import aqt.installer as i
import queue

def sequential(archives,base_dir,sevenzip,keep,archive_dest,dry_run=False):
    for a in archives:
        if a.name not in ('qtbase','icu','qtsvg'): continue
        print('Installing',a.name,flush=True)
        i.installer(a,base_dir,sevenzip,queue.Queue(),archive_dest,i.Settings.configfile,keep)
i.run_installer=sequential
raise SystemExit(i.Cli().run(['install-qt','linux','desktop','6.11.2','linux_gcc_64','-O','/tmp/telegram-qt-new']))
