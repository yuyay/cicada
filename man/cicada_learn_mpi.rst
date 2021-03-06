================
cicada_learn_mpi
================

:Author: Taro Watanabe <taro.watanabe@nict.go.jp>
:Date: 2013-8-1
:Manual section: 1

SYNOPSIS
--------

**cicada_learn_mpi** [*options*]

OPTIONS
-------

  **--forest** `arg`                           forest path(s)

  **--input** `arg`                            input path(s) (an alias for --forest)

  **--intersected** `arg`                      intersected forest path(s)

  **--oracle** `arg`                           oracle forest path(s) (an alias for 

                                        **--intersected)** 

  **--refset** `arg`                           reference translation(s)

  **--weights** `arg`                          initial parameter

  **--weights-history** `arg`                  parameter history

  **--output** `arg`                           output parameter

  **--output-objective** `arg`                 output final objective

  **--iteration** `arg (=100)`                 max # of iterations

  **--learn-softmax** Softmax objective

  **--learn-xbleu** xBLEU objective

  **--learn-mira** online MIRA algorithm

  **--learn-pa** online PA algorithm (synonym to MIRA)

  **--learn-nherd** online NHERD algorithm

  **--learn-arow** online AROW algorithm

  **--learn-cw** online CW algorithm

  **--learn-hinge** online hinge-loss objective with SGD

  **--optimize-lbfgs** LBFGS optimizer

  **--optimize-cg** CG optimizer

  **--optimize-sgd** SGD optimizer

  **--regularize-l1** `arg`                    L1-regularization

  **--regularize-l2** `arg`                    L2-regularization

  **--regularize-lambda** `arg`                regularization constant

  **--regularize-oscar** `arg`                 OSCAR regularization constant

  **--scale** `arg (=1)`                       scaling for weight

  **--alpha0** `arg (=0.84999999999999998)`    \alpha_0 for decay

  **--eta0** `arg`                             \eta_0 for decay

  **--order** `arg (=4)`                       ngram order for xBLEU

  **--rate-exponential** exponential learning rate

  **--rate-simple** simple learning rate

  **--rate-adagrad** adaptive learning rate (AdaGrad)

  **--rda** RDA method for optimization 
                                        (regularized dual averaging method)

  **--annealing** annealing

  **--quenching** quenching

  **--temperature** `arg (=0)`                 temperature

  **--temperature-start** `arg (=1000)`        start temperature for annealing

  **--temperature-end** `arg (=0.001)`         end temperature for annealing

  **--temperature-rate** `arg (=0.5)`          annealing rate

  **--quench-start** `arg (=0.01)`             start quench for annealing

  **--quench-end** `arg (=100)`                end quench for annealing

  **--quench-rate** `arg (=10)`                quenching rate

  **--scale-fixed** fixed scaling

  **--scorer** `arg (=bleu:order=4,exact=true)` 
                                        error metric

  **--scorer-list** list of error metric

  **--unite** unite forest sharing the same id

  **--debug** `[=arg(=1)]`                     debug level

  **--help** help message


