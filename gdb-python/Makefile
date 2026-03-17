.venv: 
	python3 -m venv .venv
compile:
	pip-compile pyproject.toml
install: compile
	python -m pip install -e . --no-build-isolation
upgrade:
	pip-compile --upgrade pyproject.toml
freeze:
	python -m pip freeze > requirements.txt
archive: 
	rm -f latest.tar.xz
	git archive -o latest.tar HEAD 
	git submodule foreach --recursive 'git archive -o latest.tar HEAD --prefix=$$sm_path/'
	git submodule foreach --recursive 'tar --file=$$toplevel/latest.tar --concatenate latest.tar'
	xz --keep latest.tar
