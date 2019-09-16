# Console log of main commands

## Backlog from older project

```shell
 4322  minikube --version
 4323  minikube version
 4324  yay -Syu
 4325  minikube config
 4326  minikube config view
 4327  minikube config get cpus
 4328  minikube addons enable ingress
 4329  sudo systemctl start docker minikube
 4330  whereis docker.service
 4331  ls /etc/sysctl.d
 4332  systemctl status
 4333  systemctl
 4334  minikube start
 4335  minikube config set cpus 4
 4336  minikube delete
 4337  minikube config set cpus 4
 4338  minikube config set memory 8192
 4339  minikube addons enable ingress
 4340  minikube start
 4341  minikube delete
 4342  minikube config set disk-size 5g
 4343  minikube delete
 4344  minikube start
 4345  yay -S kubectl
 4346  virtualbox --version
 4347  htop
 4348  kubectl --version
 4349  kubectl version
 4350  minikube addons enable ingress
 4351  minikube ip
 4352  minikube dashboard
 4353  minikube docker-env
 4354  eval $(minikube docker-env)
 4355  yay -S istio-bin
 4356  minikube dashboard
 4357  whereis istio
 4358  ls /usr/share/istio
 4359  export ISTIO_KUBE='/usr/share/istio/install/kubernetes'
 4360  ls /usr/share/istio/install/kubernetes/helm/istio/templates/
 4361  yay -S file-roller
 4362  file-roller
 4363  whereisfile-roller
 4364  whereis file-roller
 4365  for i in /usr/share/istio/install/kubernetes/helm/istio-init/files/crd*yaml; do kubectl apply -f $i; done
 4366  for i in /usr/share/istio/install/kubernetes/istio-demo.yaml ; do kubectl apply -f $i; done
 4367  kubectl label namespace default istio-injection=enabled
 4368  xhost +local:root
 4369  yay -S xhost
 4370  exit
 4371  yay -S xorg-xhost
 4372  xhost +local:root
 4373  docker run -it --env="DISPLAY" --volume="/tmp/.X11-unix:/tmp/.X11-unix:rw" waikato/moa:latest
 4390  kubectl --version
 4391  kubectl version
 4392  history
 4393  mkdir project/fl-minas
 4394  cd project/fl-minas
 4395  git init
 4396  code .
 4397  exit
 4398  cd project/fl-minas
 4399  exit
 4400  $ZSH/tools/check_for_upgrade.sh
 4401  exit
 4402  cd project/fl-minas
 4403  bash
 4404  yay -q ttf-ancient-fonts
 4405  yay -S ttf-ancient-fonts
 4406  exit
 4407  cd project/fl-minas
```

## 2019-09-16 log

```sh
 4420  minikube config set cpus 4
 4421  minikube config set memory 11g
 4422  minikube config set memory 11000
 4423  minikube config set disk-size 12g
 4424  minikube start
 4425  minikube version
 4426  kubectl version
 4427  kubectl cluster-info
 4428  kubectl get nodes
 4429  minikube dashboard
 4430  kubectl run kubernetes-bootcamp --image=gcr.io/google-samples/kubernetes-bootcamp:v1 --port=8080
 4431  kubectl get deployments
 4432  minikube dashboard
 4433  kubectl proxy
 4434  curl http://localhost:8001/version
 4435  export POD_NAME=$(kubectl get pods -o go-template --template '{{range .items}}{{.metadata.name}}{{"\n"}}{{end}}')
 4436  cho Name of the Pod: $POD_NAME
 4437  echo Name of the Pod: $POD_NAME
 4438  curl http://localhost:8001/api/v1/namespaces/default/pods/$POD_NAME/proxy/
 4439  echo $POD_NAME
 4440  curl http://localhost:8001/api/v1/namespaces/default/pods/kubernetes-bootcamp-5b48cfdcbd-gpzxc/proxy/
 4441  kubectl get pods -o go-template --template '{{range .items}}{{.metadata.name}}{{"\n"}}{{end}}'
 4442  kubectl get pods
 4443  htop
 4444  df -j
 4445  df -h
```

```sh
$ kubectl run kubernetes-bootcamp --image=gcr.io/google-samples/kubernetes-bootcamp:v1 --port=8080
kubectl run --generator=deployment/apps.v1 is DEPRECATED and will be removed in a future version. Use kubectl run --generator=run-pod/v1 or kubectl create instead.
deployment.apps/kubernetes-bootcamp created
```

http => "Readiness probe failed: HTTP probe failed with statuscode: 503"
