'''
based on 
https://github.com/google/jax/blob/master/examples/gaussian_process_regression.py
'''
import jax
import absl
from jax import partial
from jax.config import config
import jax.numpy as np
import jax.random as random
import jax.scipy as scipy
from jax.nn import softplus
from math import pi
import matplotlib.pyplot as plt
FLAGS = absl.flags.FLAGS


@partial(jax.jit,static_argnums=(0,))
def main(argv):

    def build_covariance_matrix(covariance_function, xx, map_to=None):
        '''Builds a covariance matrix from a covariance function and data points
        by evaluating covariance_function(xx[i],xx[j]) if map_to is None, else by
        evaluating covariance_function(xx[i],map_to[j]).
        '''
        if map_to is not None:
            return jax.vmap(lambda x: jax.vmap(lambda y: covariance_function(x,y))(xx))(map_to).T
        else:
            return jax.vmap(lambda x: jax.vmap(lambda y: covariance_function(x,y))(xx))(xx)

    def gaussian_process(params, predictors, target, test_predictors=None, compute_marginal_likelihood=False):
        noise = softplus(params['noise'])
        ampl = softplus(params['amplitude'])
        scale = softplus(params['lengthscale'])
        target = target - np.mean(target)
        predictors = predictors / scale
        
        @jax.jit
        def exp_squared(x,y):
            return np.exp(-np.sum((x - y) ** 2))
        base_cov =  build_covariance_matrix(exp_squared,predictors)
        train_cov = ampl * base_cov + np.eye(x.shape[0]) * (noise + 1.0e-6)
        LDLT = scipy.linalg.cholesky(train_cov,lower=True)
        kinvy = scipy.linalg.solve_triangular(LDLT.T,
                                          scipy.linalg.solve_triangular(LDLT,target,lower=True))
        if compute_marginal_likelihood:
            lbda = np.sum( - 0.5 * np.dot(target.T,kinvy) -
                           np.sum(np.log(np.diag(LDLT))) - x.shape[0] * 0.5 * np.log(2 * pi))
            return - (lbda - np.sum(-0.5*np.log(2 * pi) - np.log(ampl) ** 2))
        
        if test_predictors is not None:
            test_predictors = test_predictors / scale
            cross_cov = ampl * build_covariance_matrix(exp_squared,predictors,test_predictors)
        else:
            cross_cov = base_cov
            
        mu = np.dot(cross_cov.T,kinvy) + np.mean(target)
        v = scipy.linalg.solve_triangular(LDLT,cross_cov,lower=True)
        cov = ampl * build_covariance_matrix(exp_squared,test_predictors) - np.dot(v.T,v)
        return mu, cov

    marginal_likelihood = partial(gaussian_process,compute_marginal_likelihood=True)
    predict = partial(gaussian_process,compute_marginal_likelihood=False)
    ml_grad = jax.jit(jax.grad(marginal_likelihood))
    
    def train_gp(predictors, target, steps=2000, default_pars = None, **kwargs):
        params = default_pars or {'amplitude' : np.ones((1,1)),
                                  'noise' : np.zeros((1,1)) - 0.5,
                                  'lengthscale' : np.ones((1,1))}
        momentum = dict((k,0.0) for k in params)
        scales = dict((k,1.0) for k in params)
        learning_rate = kwargs.get('learning_rate',0.01)
        nesterov_momentum = min(1.0,kwargs.get('momentum',0.9))
        grad_tol = abs(kwargs.get('tol',1.0e-6))
        def train_step(params,momentums,scales,x,y):
            grads = ml_grad(params,x,y)
            #grads_small = True
            for k in params:
                momentums[k] = nesterov_momentum * momentums[k] + (1.0 - nesterov_momentum) * grads[k][0]
                scales[k] = nesterov_momentum * scales[k] + (1.0 - nesterov_momentum) * grads[k][0] ** 2
                params[k] -= learning_rate * momentums[k] / np.sqrt(scales[k] + 1.0e-6)
                #if np.linalg.norm(grads[k]) > grad_tol:
                #    grads_small = False
            return params, momentums, scales#, grads_small
            
        debug_print = kwargs.get('debug',True)
        print_every = kwargs.get('print_every',50)
        
        for i in range(steps):
            params, momentum, scales = train_step(params, momentum, scales, predictors, target)
            if debug_print and i % print_every == 0:
                print(f"Step: {i}. Marginal Likelihood: {-marginal_likelihood(params,predictors,target)}")
            #if grads_small:
            #    break
        if debug_print:
            print(f"Final parameters: \n{params}")
                        
        return params
                    
    def plot_gp_test(params, predictors, target, test_predictors, conf_stds=2.0):
        mu, cov = predict(params, predictors, target, test_predictors)
        std = np.sqrt(np.diag(cov))
        plt.plot(test_predictors,mu,color='green')
        plt.plot(predictors,target,'k.')
        #plt.label()
        plt.fill_between(test_predictors.flatten(),mu.flatten() - 2*std, mu.flatten() + 2 * std)
        plt.show()
    n = FLAGS.num_points
    key = random.PRNGKey(0)
    x = (random.uniform(key,shape=(n,1)) * 4.0) + 1
    y = lambda x: np.sin(x ** 3) + 0.1 * random.normal(key,shape=(x.shape[0],1))
    yt = np.sin(x ** 3)
    yvals = y(x)
    x_test = np.linspace(1, 5, 4 * n)[:,None]
    optimal_params = train_gp(x,yvals,steps=FLAGS.maxiter,
                              default_pars={'amplitude' : np.ones((1,1)),
                                            'noise' : np.zeros((1,1)) - 5.,
                                            'lengthscale' : 0.01 * np.ones((1,1))},
                              learning_rate=FLAGS.learning_rate
    )
    plt.plot(x,yt,'r.',label='true values')
    plot_gp_test(optimal_params,x,yvals,x_test)


if __name__ == '__main__':
    absl.flags.DEFINE_integer('num_points',100,'How many observations?')
    absl.flags.DEFINE_float('learning_rate',0.01,'Learning rate for the GP')
    absl.flags.DEFINE_integer('maxiter',2000,'Maximum number of training iterations')
    config.config_with_absl()
    absl.app.run(main)
