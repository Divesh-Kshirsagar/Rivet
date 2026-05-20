import os
import subprocess
import shutil

def on_pre_build(config, **kwargs):
    print("--- Running Doxygen Pre-Build Hook ---")
    
    # We are usually in the docs directory or project root when mkdocs is run,
    # but let's carefully compute the paths based on this script's location.
    # config['config_file_path'] gives the absolute path to mkdocs.yml
    mkdocs_dir = os.path.dirname(os.path.abspath(config['config_file_path']))
    root_dir = os.path.abspath(os.path.join(mkdocs_dir, ".."))
    
    print(f"Project root: {root_dir}")
    
    # 1. Run Doxygen from the root directory so `include` and `src` paths resolve properly
    try:
        subprocess.run(["doxygen", "docs/doxygen/Doxyfile"], cwd=root_dir, check=True)
    except FileNotFoundError:
        print("Doxygen executable not found. Skipping Doxygen build.")
        return
    except subprocess.CalledProcessError as e:
        print(f"Failed to run Doxygen: {e}")
        return
        
    # 2. Move Doxygen HTML output into the mkdocs docs folder
    dox_output = os.path.join(root_dir, "docs", "doxygen", "_build", "html")
    api_dest = os.path.join(root_dir, "docs", "docs", "api")
    
    if os.path.exists(api_dest):
        shutil.rmtree(api_dest)
        
    shutil.copytree(dox_output, api_dest)
    print("--- Doxygen generation and copy complete ---")
