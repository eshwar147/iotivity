echo "Creating Temporary Directory InterOpAppRI"
echo "Please wait. It will take a few seconds..."

push="false"
iotivity_dir="iotivity"
release='1'
rpmPath=''
deviceId=''
stand_alone='false'
security_mode='justworks'

for i in `seq 1 $#` 
do
    eval arg=\$$i
    arg=${arg// /+}
    args+=$arg" "
done

arg_parts=(${args//=/ })
len=${#arg_parts[@]}

i=0
while [ $i -lt $len ]; do
    arg_parts[i]=${arg_parts[i],,}
    let i=i+2
done

i=0
while [ $i -lt $len ]; do
    if [[ "${arg_parts[i]}" = "device_id" ]]; then
        deviceId="-s "${arg_parts[i+1]}
    elif [[ "${arg_parts[i]}" = "push" ]]; then
        push=${arg_parts[i+1],,}
    elif [[ "${arg_parts[i]}" = "iotivity_dir" ]]; then
        iotivity_dir=${arg_parts[i+1]}
    elif [[ "${arg_parts[i]}" = "rpms_dir" ]]; then
        rpmPath=${arg_parts[i+1]}
    elif [[ "${arg_parts[i]}" = "stand_alone" ]]; then
        stand_alone=${arg_parts[i+1],,}
    elif [[ "${arg_parts[i]}" = "security_mode" ]]; then
        security_mode=${arg_parts[i+1],,}
    fi
    let i=i+2
done

if [[ "${stand_alone}" = "false" ]] || [[ "${stand_alone}" = "0" ]]; then
    echo 'stand_alone pwd: '`pwd`
    cd build/tizen/iori
fi    

tmpDir='tmp_iori'
target_arch='armv7l'
rpmName="com-oic-iori-0.0.1-1.armv7l.rpm"

current_path=`pwd`
echo 'current_path: '$current_path

cd ../../../
test_project_root=`pwd`
echo 'test_project_root: '$test_project_root
    
cd ..
iotivity_path=`pwd`
echo 'iotivity_path: '$iovitiy_path

cd $current_path

echo $tmpDir
echo $homeDir
pwd

rm -rf $tmpDir/

mkdir $tmpDir
mkdir $tmpDir/packaging
mkdir -p $tmpDir'/bin/tizen/iori'
mkdir -p $tmpDir/iotivity/resource
mkdir -p $tmpDir/test/src
mkdir -p $tmpDir/test/include

cp -R $iotivity_path/resource/* $tmpDir/iotivity/resource/

cp -R $test_project_root/src $tmpDir/test
cp -R $test_project_root/include $tmpDir/test

cp com.oic.iori.spec $tmpDir/packaging
cp com.oic.iori.manifest $tmpDir
cp com.oic.iori.xml $tmpDir
cp SConstruct $tmpDir

echo "Initializing Git repository"
echo " "

cd $tmpDir

echo 'pwd: '`pwd`

if [ ! -d .git ]; then
   git init ./
   git config user.email "you@example.com"
   git config user.name "Your Name"
   git add ./
   git commit -m "Initial commit"
fi

rm $rpmPath'/'$rpmName

echo "Calling core gbs build command"

gbscommand="gbs build -A "$target_arch" -B ~/GBS-ROOT-OIC --include-all --define 'TARGET_TRANSPORT ALL' --define 'SECURED 1'"
echo $gbscommand

echo "Starting GBS Build ..."
echo $rpmPath

if eval $gbscommand; then
   echo "GBS build is successful"   
   echo "Extracting RPM"
   extractCommand="rpm2cpio "$rpmPath/$rpmName" | cpio -idmv"
   echo $extractCommand

   chmod 777 $rpmPath/$rpmName
   sleep 1
   eval $extractCommand
   echo $(pwd)
   echo "Copying binary files"
   mkdir -p ../$targetDir
   cp usr/apps/com.oic.iori/bin/* ../$targetDir

    echo "Copying RPM"
    cp $rpmPath/$rpmName ../$targetDir

   chmod 777 -R ../$targetDir
else
   echo "GBS build failed."
fi
echo ""
echo "Deleting Tmp Directory"

cd ..

if [ -f $rpmPath'/'$rpmName ]; then
    echo 'Build Successful'
else    
    echo 'Build Failed!  File '$rpmPath'/'$rpmName' Not Found'
    exit 127
fi

rm -rf $tmpDir/

if [[ "${push}" = "true" ]] || [[ "${push}" = "1" ]]; then
    echo 'pushing rpm to tizen device ..'

    sdb $deviceId push $rpmPath'/'$rpmName /tmp
    sdb $deviceId root on
    sdb $deviceId shell rpm -Uvh 'tmp/'$rpmName --force --nodeps

    if [[ "${security_mode}" = "justworks" ]]; then
        db_filename='oic_svr_db_server_justworks.dat'
    elif [[ "${arg_parts[i]}" = "randompin" ]]; then
        db_filename='oic_svr_db_server_randompin.dat'
    fi
    
    sdb $deviceId push $iotivity_path/resource/csdk/security/provisioning/sample/oic_svr_db_server_justworks.dat /usr/apps/com.oic.iori/bin/oic_svr_db_server.dat
    sdb $deviceId push $iotivity_path/resource/csdk/security/provisioning/sample/oic_svr_db_server_randompin.dat /usr/apps/com.oic.iori/bin/oic_svr_db_server_randompin.dat
    
fi

mkdir -p $test_project_root/bin/tizen/iori
mv $test_project_root/build/tizen/iori/$rpmName $test_project_root/bin/tizen/iori/$rpmName
mv $test_project_root/build/tizen/iori/InterOpAppRI $test_project_root/bin/tizen/iori/InterOpAppRI

if [[ "${stand_alone}" = "false" ]] || [[ "${stand_alone}" = "0" ]]; then
    cd ../../../
fi    
