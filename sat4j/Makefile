jar = ./target/org.sat4j.moco.threeAlgorithms-0.0.1-SNAPSHOT.jar
source = $(shell find  ./src -type f -name *.java)
build: $(jar)

$(jar): $(source)
	mvn -T1C -o -Dmaven.test.skip -DskipTests package
clean:
	mvn clean
archive: 
	rm -f latest.tar.xz
	git archive -o latest.tar HEAD 
	git submodule foreach --recursive 'git archive -o latest.tar HEAD --prefix=$$sm_path/'
	git submodule foreach --recursive 'tar --file=$$toplevel/latest.tar --concatenate latest.tar'
	xz --keep latest.tar
